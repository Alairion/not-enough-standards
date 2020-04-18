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

#ifndef NOT_ENOUGH_STANDARDS_NAMED_SEMAPHORE
#define NOT_ENOUGH_STANDARDS_NAMED_SEMAPHORE

#if defined(_WIN32)
    #define NES_WIN32_NAMED_SEMAPHORE
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #define NES_POSIX_NAMED_SEMAPHORE
    #include <unistd.h>
    #include <string.h>
    #include <semaphore.h>
    #include <fcntl.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <string>
#include <chrono>
#include <limits>
#include <utility>
#include <stdexcept>

#if defined(NES_WIN32_NAMED_SEMAPHORE)

namespace nes
{

inline constexpr const char named_semaphore_root[] = u8"Local\\";

class named_semaphore
{
public:
    using native_handle_type = HANDLE;

public:
    explicit named_semaphore(const std::string& name, std::size_t initial_count = 0)
    {
        const auto native_name{to_wide(named_semaphore_root + name)};

        m_handle = CreateSemaphoreW(nullptr, static_cast<LONG>(initial_count), std::numeric_limits<LONG>::max(), std::data(native_name));
        if(!m_handle)
        {
            if(GetLastError() == ERROR_ACCESS_DENIED)
            {
                m_handle = OpenSemaphoreW(SYNCHRONIZE, FALSE, std::data(native_name));
                if(!m_handle)
                    throw std::runtime_error{"Failed to open semaphore. " + get_error_message()};
            }
            else
            {
                throw std::runtime_error{"Failed to create semaphore. " + get_error_message()};
            }
        }
    }

    ~named_semaphore()
    {
        CloseHandle(m_handle);
    }

    named_semaphore(const named_semaphore&) = delete;
    named_semaphore& operator=(const named_semaphore&) = delete;
    named_semaphore(named_semaphore&& other) noexcept = delete;
    named_semaphore& operator=(named_semaphore&& other) noexcept = delete;

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
    std::wstring to_wide(const std::string& path)
    {
        if(std::empty(path))
            return {};

        std::wstring out_path{};
        out_path.resize(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, 0, std::data(path), static_cast<int>(std::size(path)), nullptr, 0)));

        if (!MultiByteToWideChar(CP_UTF8, 0, std::data(path), static_cast<int>(std::size(path)), std::data(out_path), static_cast<int>(std::size(out_path))))
            throw std::runtime_error{"Failed to convert the path to wide."};

        return out_path;
    }

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

class timed_named_semaphore
{
public:
    using native_handle_type = HANDLE;

public:
    explicit timed_named_semaphore(const std::string& name, std::size_t initial_count = 0)
    {
        const auto native_name{to_wide(named_semaphore_root + name)};

        m_handle = CreateSemaphoreW(nullptr, static_cast<LONG>(initial_count), std::numeric_limits<LONG>::max(), std::data(native_name));
        if(!m_handle)
        {
            if(GetLastError() == ERROR_ACCESS_DENIED)
            {
                m_handle = OpenSemaphoreW(SYNCHRONIZE, FALSE, std::data(native_name));
                if(!m_handle)
                    throw std::runtime_error{"Failed to open semaphore. " + get_error_message()};
            }
            else
            {
                throw std::runtime_error{"Failed to create semaphore. " + get_error_message()};
            }
        }
    }

    ~timed_named_semaphore()
    {
        CloseHandle(m_handle);
    }

    timed_named_semaphore(const timed_named_semaphore&) = delete;
    timed_named_semaphore& operator=(const timed_named_semaphore&) = delete;
    timed_named_semaphore(timed_named_semaphore&& other) noexcept = delete;
    timed_named_semaphore& operator=(timed_named_semaphore&& other) noexcept = delete;

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
    std::wstring to_wide(const std::string& path)
    {
        if(std::empty(path))
            return {};

        std::wstring out_path{};
        out_path.resize(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, 0, std::data(path), static_cast<int>(std::size(path)), nullptr, 0)));

        if (!MultiByteToWideChar(CP_UTF8, 0, std::data(path), static_cast<int>(std::size(path)), std::data(out_path), static_cast<int>(std::size(out_path))))
            throw std::runtime_error{"Failed to convert the path to wide."};

        return out_path;
    }

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

#elif defined(NES_POSIX_NAMED_SEMAPHORE)

namespace nes
{

inline constexpr const char named_semaphore_root[] = u8"/";

class named_semaphore
{
public:
    using native_handle_type = sem_t*;

public:
    explicit named_semaphore(const std::string& name, std::size_t initial_count = 0)
    {
        const auto native_name{named_semaphore_root + name};

        m_handle = sem_open(std::data(native_name), O_CREAT, 0660, initial_count);
        if(m_handle == SEM_FAILED)
            throw std::runtime_error{"Failed to create semaphore. " + std::string{strerror(errno)}};
    }

    ~named_semaphore()
    {
        sem_close(m_handle);
    }

    named_semaphore(const named_semaphore&) = delete;
    named_semaphore& operator=(const named_semaphore&) = delete;
    named_semaphore(named_semaphore&& other) noexcept = delete;
    named_semaphore& operator=(named_semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(sem_wait(m_handle) == -1)
            throw std::runtime_error{"Failed to decrement semaphore count. " + std::string{strerror(errno)}};
    }

    bool try_acquire()
    {
        return !sem_trywait(m_handle);
    }

    void release()
    {
        if(sem_post(m_handle) == -1)
            throw std::runtime_error{"Failed to increment semaphore count. " + std::string{strerror(errno)}};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    native_handle_type m_handle{};
};

class timed_named_semaphore
{
public:
    using native_handle_type = sem_t*;

public:
    explicit timed_named_semaphore(const std::string& name, std::size_t initial_count = 0)
    {
        const auto native_name{named_semaphore_root + name};

        m_handle = sem_open(std::data(native_name), O_CREAT, 0660, initial_count);
        if(m_handle == SEM_FAILED)
            throw std::runtime_error{"Failed to create semaphore. " + std::string{strerror(errno)}};
    }

    ~timed_named_semaphore()
    {
        sem_close(m_handle);
    }

    timed_named_semaphore(const timed_named_semaphore&) = delete;
    timed_named_semaphore& operator=(const timed_named_semaphore&) = delete;
    timed_named_semaphore(timed_named_semaphore&& other) noexcept = delete;
    timed_named_semaphore& operator=(timed_named_semaphore&& other) noexcept = delete;

    void acquire()
    {
        if(sem_wait(m_handle) == -1)
            throw std::runtime_error{"Failed to decrement semaphore count. " + std::string{strerror(errno)}};
    }

    bool try_acquire()
    {
        return !sem_trywait(m_handle);
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

        return !sem_timedwait(m_handle, &time);
    }

    void release()
    {
        if(sem_post(m_handle) == -1)
            throw std::runtime_error{"Failed to increment semaphore count. " + std::string{strerror(errno)}};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    native_handle_type m_handle{};
};

}

#endif

#endif
