///////////////////////////////////////////////////////////
/// Copyright 2019 Alexy Pellegrini
///
/// Permission is hereby granted, free of charge,
/// to any person obtaining a copy of this software
/// and associated documentation files (the "Software"),
/// to deal in the Software without restriction,
/// including without limitation the rights to use,
/// copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so,
/// subject to the following conditions:
///
/// The above copyright notice and this permission notice
/// shall be included in all copies or substantial portions
/// of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
/// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
/// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
/// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
/// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
/// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
/// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
/// OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////

#ifndef NOT_ENOUGH_STANDARDS_SEMAPHORE
#define NOT_ENOUGH_STANDARDS_SEMAPHORE

#if defined(_WIN32)
    #define NES_WIN32_SEMAPHORE
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #define NES_POSIX_SEMAPHORE
    #include <unistd.h>
    #include <string.h>
    #include <semaphore.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <string>
#include <chrono>
#include <limits>
#include <utility>
#include <stdexcept>
#include <memory>

#if defined(NES_WIN32_SEMAPHORE)

namespace nes
{

class semaphore
{
public:
    using native_handle_type = HANDLE;

public:
    explicit semaphore(std::size_t initial_count = 0)
    {
        m_handle = CreateSemaphoreW(nullptr, static_cast<LONG>(initial_count), std::numeric_limits<LONG>::max(), nullptr);
        if(!m_handle)
            throw std::runtime_error{"Failed to create semaphore. " + get_error_message()};
    }

    ~semaphore()
    {
        if(m_handle)
            CloseHandle(m_handle);
    }

    semaphore(const semaphore&) = delete;
    semaphore& operator=(const semaphore&) = delete;
    semaphore(semaphore&& other) noexcept = delete;
    semaphore& operator=(semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(WaitForSingleObject(m_handle, INFINITE))
            throw std::runtime_error{"Failed to decrement semaphore count. " + get_error_message()};
    }

    bool try_acquire()
    {
        return WaitForSingleObject(m_handle, 0) == WAIT_OBJECT_0;
    }

    void release()
    {
        if(!ReleaseSemaphore(m_handle, 1, nullptr))
            throw std::runtime_error{"Failed to increment semaphore count. " + get_error_message()};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    std::string get_error_message() const
    {
        std::string out{};
        out.resize(1024);

        const DWORD error{GetLastError()};
        const DWORD out_size{FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, std::data(out), static_cast<DWORD>(std::size(out)), nullptr)};
        out.resize(std::max(out_size - 2, DWORD{}));

        out += " (#" + std::to_string(error) + ")";

        return out;
    }

private:
    native_handle_type m_handle{};
};

class timed_semaphore
{
public:
    using native_handle_type = HANDLE;

public:
    explicit timed_semaphore(std::size_t initial_count = 0)
    {
        m_handle = CreateSemaphoreW(nullptr, static_cast<LONG>(initial_count), std::numeric_limits<LONG>::max(), nullptr);
        if(!m_handle)
            throw std::runtime_error{"Failed to create semaphore. " + get_error_message()};
    }

    ~timed_semaphore()
    {
        CloseHandle(m_handle);
    }

    timed_semaphore(const timed_semaphore&) = delete;
    timed_semaphore& operator=(const timed_semaphore&) = delete;
    timed_semaphore(timed_semaphore&& other) noexcept = delete;
    timed_semaphore& operator=(timed_semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(WaitForSingleObject(m_handle, INFINITE))
            throw std::runtime_error{"Failed to decrement semaphore count. " + get_error_message()};
    }

    bool try_acquire()
    {
        return WaitForSingleObject(m_handle, 0) == WAIT_OBJECT_0;
    }

    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return WaitForSingleObject(m_handle, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()) == WAIT_OBJECT_0;
    }

    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        const auto current_time{Clock::now()};
        if(time_point < current_time)
            return try_acquire();

        return try_acquire_for(time_point - current_time);
    }

    void release()
    {
        if(!ReleaseSemaphore(m_handle, 1, nullptr))
            throw std::runtime_error{"Failed to increment semaphore count. " + get_error_message()};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    std::string get_error_message() const
    {
        std::string out{};
        out.resize(1024);

        const DWORD error{GetLastError()};
        const DWORD out_size{FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, std::data(out), static_cast<DWORD>(std::size(out)), nullptr)};
        out.resize(std::max(out_size - 2, DWORD{}));

        out += " (#" + std::to_string(error) + ")";

        return out;
    }

private:
    native_handle_type m_handle{};
};

}

#elif defined(NES_POSIX_SEMAPHORE)

namespace nes
{

class semaphore
{
public:
    using native_handle_type = sem_t*;

public:
    explicit semaphore(std::size_t initial_count = 0)
    {
        if(sem_init(m_handle.get(), 0, initial_count) != 0)
            throw std::runtime_error{"Failed to create semaphore. " + std::string{strerror(errno)}};
    }

    ~semaphore()
    {
        sem_destroy(m_handle.get());
    }

    semaphore(const semaphore&) = delete;
    semaphore& operator=(const semaphore&) = delete;
    semaphore(semaphore&& other) noexcept = delete;
    semaphore& operator=(semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(sem_wait(m_handle.get()) == -1)
            throw std::runtime_error{"Failed to decrement semaphore count. " + std::string{strerror(errno)}};
    }

    bool try_acquire()
    {
        return !sem_trywait(m_handle.get());
    }

    void release()
    {
        if(sem_post(m_handle.get()) == -1)
            throw std::runtime_error{"Failed to increment semaphore count. " + std::string{strerror(errno)}};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle.get();
    }

private:
    std::unique_ptr<sem_t> m_handle{std::make_unique<sem_t>()};
};

class timed_semaphore
{
public:
    using native_handle_type = sem_t*;

public:
    explicit timed_semaphore(std::size_t initial_count = 0)
    {
        if(sem_init(m_handle.get(), 0, initial_count) != 0)
            throw std::runtime_error{"Failed to create timed_semaphore. " + std::string{strerror(errno)}};
    }

    ~timed_semaphore()
    {
        sem_destroy(m_handle.get());
    }

    timed_semaphore(const timed_semaphore&) = delete;
    timed_semaphore& operator=(const timed_semaphore&) = delete;
    timed_semaphore(timed_semaphore&& other) noexcept = delete;
    timed_semaphore& operator=(timed_semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(sem_wait(m_handle.get()) == -1)
            throw std::runtime_error{"Failed to decrement semaphore count. " + std::string{strerror(errno)}};
    }

    bool try_acquire()
    {
        return !sem_trywait(m_handle.get());
    }

    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return try_lock_until(std::chrono::system_clock::now() + timeout);
    }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        const auto seconds{std::chrono::time_point_cast<std::chrono::seconds>(time_point)};
        const auto nanoseconds{std::chrono::duration_cast<std::chrono::nanoseconds>(time_point - seconds)};

        timespec time{};
        time.tv_sec = static_cast<std::time_t>(seconds.time_since_epoch().count());
        time.tv_nsec = static_cast<long>(nanoseconds.count());

        return !sem_timedwait(m_handle.get(), &time);
    }

    void release()
    {
        if(sem_post(m_handle.get()) == -1)
            throw std::runtime_error{"Failed to increment semaphore count. " + std::string{strerror(errno)}};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle.get();
    }

private:
    std::unique_ptr<sem_t> m_handle{std::make_unique<sem_t>()};
};

}

#endif

#endif
