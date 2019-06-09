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

#ifndef NOT_ENOUGH_STANDARDS_NAMED_MUTEX
#define NOT_ENOUGH_STANDARDS_NAMED_MUTEX

#if __has_include(<windows.h>)
    #define NES_WIN32_NAMED_MUTEX
    #include <windows.h>
#elif __has_include(<unistd.h>)
    #define NES_POSIX_NAMED_MUTEX
    #include <unistd.h>
    #include <pthread.h>
    #include <time.h>
    #include <fcntl.h>
    #include <string.h>
    #include <sys/mman.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <string>
#include <utility>
#include <stdexcept>
#include <cassert>
#include <chrono>

#if defined(NES_WIN32_NAMED_MUTEX)

namespace nes
{

static constexpr const char* named_mutex_root = u8"Local\\";

namespace impl
{

struct named_mutex_base
{
    HANDLE create_or_open(const std::string& name)
    {
        const auto native_name{to_wide(named_mutex_root + name)};

        HANDLE handle{CreateMutexW(nullptr, FALSE, std::data(native_name))};
        if(!handle)
        {
            if(GetLastError() == ERROR_ACCESS_DENIED)
            {
                handle = OpenMutexW(SYNCHRONIZE, FALSE, std::data(native_name));
                if(!handle)
                    throw std::runtime_error{"Can not open named mutex. " + get_error_message()};
            }
            else
            {
                throw std::runtime_error{"Can not create named mutex. " + get_error_message()};
            }
        }

        return handle;
    }

    std::wstring to_wide(const std::string& path)
    {
        if(std::empty(path))
            return {};

        std::wstring out_path{};
        const auto required_size = MultiByteToWideChar(CP_UTF8, 0, std::data(path), std::size(path), nullptr, 0);
        out_path.resize(required_size);

        if(!MultiByteToWideChar(CP_UTF8, 0, std::data(path), std::size(path), std::data(out_path), std::size(out_path)))
            throw std::runtime_error{"Can not convert the path to wide."};

        return out_path;
    }

    std::string get_error_message() const
    {
        std::string out{};
        out.resize(1024);

        const DWORD error{GetLastError()};
        const DWORD out_size{FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, std::data(out), std::size(out), nullptr)};
        out.resize(std::max(out_size - 2, DWORD{}));

        out += " (#" + std::to_string(error) + ")";

        return out;
    }
};

}

class named_mutex : impl::named_mutex_base
{
public:
    using native_handle_type = HANDLE;

public:
    explicit named_mutex(const std::string& name)
    :m_handle{create_or_open(name)}
    {

    }

    ~named_mutex()
    {
        CloseHandle(m_handle);
    }

    named_mutex(const named_mutex&) = delete;
    named_mutex& operator=(const named_mutex&) = delete;
    named_mutex(named_mutex&&) noexcept = delete;
    named_mutex& operator=(named_mutex&&) noexcept = delete;

    void lock()
    {
         if(WaitForSingleObject(m_handle, INFINITE) == WAIT_FAILED)
             throw std::runtime_error{"Failed to lock mutex. " + get_error_message()};
    }

    bool try_lock()
    {
        return WaitForSingleObject(m_handle, 0) == WAIT_OBJECT_0;
    }

    void unlock()
    {
        ReleaseMutex(m_handle);
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    native_handle_type m_handle{};
};

class timed_named_mutex : impl::named_mutex_base
{
public:
    using native_handle_type = HANDLE;

public:
    explicit timed_named_mutex(const std::string& name)
    :m_handle{create_or_open(name)}
    {

    }

    ~timed_named_mutex()
    {
        CloseHandle(m_handle);
    }

    timed_named_mutex(const timed_named_mutex&) = delete;
    timed_named_mutex& operator=(const timed_named_mutex&) = delete;
    timed_named_mutex(timed_named_mutex&&) noexcept = delete;
    timed_named_mutex& operator=(timed_named_mutex&&) noexcept = delete;

    void lock()
    {
        if(WaitForSingleObject(m_handle, INFINITE) == WAIT_FAILED)
            throw std::runtime_error{"Failed to lock mutex. " + get_error_message()};
    }

    bool try_lock()
    {
        return WaitForSingleObject(m_handle, 0) == WAIT_OBJECT_0;
    }

    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return WaitForSingleObject(m_handle, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()) == WAIT_OBJECT_0;
    }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        const auto current_time{Clock::now()};
        if(time_point < current_time)
            return try_lock();

        return try_lock_for(time_point - current_time);
    }

    void unlock()
    {
        ReleaseMutex(m_handle);
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    native_handle_type m_handle{};
};

class recursive_named_mutex : public named_mutex
{

};

class recursive_timed_named_mutex : public timed_named_mutex
{

};

}

#elif defined(NES_POSIX_NAMED_MUTEX)


namespace nes
{

static constexpr const char* named_mutex_root = u8"/";

namespace impl
{

struct mutex_data
{
    std::uint64_t opened{};
    pthread_mutex_t mutex{};
};

struct mutex_base
{
    int memory{-1};
    mutex_data* data{};
};

inline mutex_base create_or_open_mutex(const std::string& name, bool recursive)
{
    const auto native_name{named_mutex_root + name};

    int shm_handle{shm_open(std::data(native_name), O_RDWR | O_CREAT, 0660)};
    if(shm_handle == -1)
        throw std::runtime_error{"Can not allocate space for named mutex. " + std::string{strerror(errno)}};

    if(ftruncate(shm_handle, sizeof(mutex_data)) == -1)
    {
        close(shm_handle);
        throw std::runtime_error{"Can not truncate shared memory for named mutex. " + std::string{strerror(errno)}};
    }

    auto* ptr{reinterpret_cast<mutex_data*>(mmap(nullptr, sizeof(mutex_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handle, 0))};
    if(ptr == MAP_FAILED)
    {
        close(shm_handle);
        throw std::runtime_error{"Can not map shared memory for named mutex. " + std::string{strerror(errno)}};
    }

    if(!ptr->opened)
    {
        pthread_mutexattr_t attr{};
        pthread_mutexattr_init(&attr);

        auto clean_and_throw = [ptr, shm_handle, &attr](const std::string& error_str, int error)
        {
            munmap(ptr, sizeof(mutex_data));
            close(shm_handle);
            pthread_mutexattr_destroy(&attr);
            throw std::runtime_error{error_str + std::string{strerror(error)}};
        };

        if(auto error = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); error != 0)
            clean_and_throw("Can not set process shared attribute of mutex. ", error);

        if(auto error = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST); error != 0)
            clean_and_throw("Can not set robust attribute of mutex. ", error);

        if(recursive)
            if(auto error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); error != 0)
                clean_and_throw("Can not set recursive attribute of mutex. ", error);

        if(auto error = pthread_mutex_init(&ptr->mutex, &attr); error != 0)
            clean_and_throw("Can not init mutex. ", error);

        pthread_mutexattr_destroy(&attr);

        ptr->opened = 1;
    }

    return mutex_base{shm_handle, ptr};
}

inline void close_mutex(mutex_base& mutex)
{
    if(mutex.data)
        munmap(std::exchange(mutex.data, nullptr), sizeof(mutex_data));
    if(mutex.memory != -1)
        close(std::exchange(mutex.memory, -1));
}

inline void lock_mutex(mutex_base& mutex)
{
    auto error{pthread_mutex_lock(&mutex.data->mutex)};
    if(error == EOWNERDEAD)
        pthread_mutex_consistent(&mutex.data->mutex);
    else if(error != 0)
        throw std::runtime_error{"Can not lock mutex. " +  std::string{strerror(error)}};
}

inline bool try_lock_mutex(mutex_base& mutex)
{
    auto error{pthread_mutex_trylock(&mutex.data->mutex)};
    if(error == EOWNERDEAD)
    {
        pthread_mutex_consistent(&mutex.data->mutex);
        return true;
    }

    return !error;
}

inline bool try_lock_mutex_until(mutex_base& mutex, const timespec& time)
{
    auto error{pthread_mutex_timedlock(&mutex.data->mutex, &time)};
    if(error == EOWNERDEAD)
    {
        pthread_mutex_consistent(&mutex.data->mutex);
        return true;
    }

    return !error;
}

}

class named_mutex
{
public:
    using native_handle_type = pthread_mutex_t*;

public:
    explicit named_mutex(const std::string& name)
    :m_handle{impl::create_or_open_mutex(name, false)}
    {

    }

    ~named_mutex()
    {
        impl::close_mutex(m_handle);
    }

    named_mutex(const named_mutex&) = delete;
    named_mutex& operator=(const named_mutex&) = delete;
    named_mutex(named_mutex&&) noexcept = delete;
    named_mutex& operator=(named_mutex&&) noexcept = delete;

    void lock()
    {
        impl::lock_mutex(m_handle);
    }

    bool try_lock()
    {
        return impl::try_lock_mutex(m_handle);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_handle.data->mutex);
    }

    native_handle_type native_handle() const noexcept
    {
        return &m_handle.data->mutex;
    }

private:
    impl::mutex_base m_handle{};
};

class timed_named_mutex
{
public:
    using native_handle_type = pthread_mutex_t*;

public:
    explicit timed_named_mutex(const std::string& name)
    :m_handle{impl::create_or_open_mutex(name, false)}
    {

    }

    ~timed_named_mutex()
    {
        impl::close_mutex(m_handle);
    }

    timed_named_mutex(const timed_named_mutex&) = delete;
    timed_named_mutex& operator=(const timed_named_mutex&) = delete;
    timed_named_mutex(timed_named_mutex&&) noexcept = delete;
    timed_named_mutex& operator=(timed_named_mutex&&) noexcept = delete;

    void lock()
    {
        impl::lock_mutex(m_handle);
    }

    bool try_lock()
    {
        return impl::try_lock_mutex(m_handle);
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

        return impl::try_lock_mutex_until(m_handle, time);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_handle.data->mutex);
    }

    native_handle_type native_handle() const noexcept
    {
        return &m_handle.data->mutex;
    }

private:
    impl::mutex_base m_handle{};
};

class recursive_named_mutex
{
public:
    using native_handle_type = pthread_mutex_t*;

public:
    explicit recursive_named_mutex(const std::string& name)
    :m_handle{impl::create_or_open_mutex(name, true)}
    {

    }

    ~recursive_named_mutex()
    {
        impl::close_mutex(m_handle);
    }

    recursive_named_mutex(const recursive_named_mutex&) = delete;
    recursive_named_mutex& operator=(const recursive_named_mutex&) = delete;
    recursive_named_mutex(recursive_named_mutex&&) noexcept = delete;
    recursive_named_mutex& operator=(recursive_named_mutex&&) noexcept = delete;

    void lock()
    {
        impl::lock_mutex(m_handle);
    }

    bool try_lock()
    {
        return impl::try_lock_mutex(m_handle);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_handle.data->mutex);
    }

    native_handle_type native_handle() const noexcept
    {
        return &m_handle.data->mutex;
    }

private:
    impl::mutex_base m_handle{};
};

class recursive_timed_named_mutex
{
public:
    using native_handle_type = pthread_mutex_t*;

public:
    explicit recursive_timed_named_mutex(const std::string& name)
    :m_handle{impl::create_or_open_mutex(name, true)}
    {

    }

    ~recursive_timed_named_mutex()
    {
        impl::close_mutex(m_handle);
    }

    recursive_timed_named_mutex(const recursive_timed_named_mutex&) = delete;
    recursive_timed_named_mutex& operator=(const recursive_timed_named_mutex&) = delete;
    recursive_timed_named_mutex(recursive_timed_named_mutex&&) noexcept = delete;
    recursive_timed_named_mutex& operator=(recursive_timed_named_mutex&&) noexcept = delete;

    void lock()
    {
        impl::lock_mutex(m_handle);
    }

    bool try_lock()
    {
        return impl::try_lock_mutex(m_handle);
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

        return impl::try_lock_mutex_until(m_handle, time);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_handle.data->mutex);
    }

    native_handle_type native_handle() const noexcept
    {
        return &m_handle.data->mutex;
    }

private:
    impl::mutex_base m_handle{};
};

}

#endif

#endif
