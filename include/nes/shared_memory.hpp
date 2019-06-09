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

#ifndef NOT_ENOUGH_STANDARDS_SHARED_MEMORY
#define NOT_ENOUGH_STANDARDS_SHARED_MEMORY

#if __has_include(<windows.h>)
    #define NES_WIN32_SHARED_MEMORY
    #include <windows.h>
#elif __has_include(<unistd.h>)
    #define NES_POSIX_SHARED_MEMORY
    #include <unistd.h>
    #include <fcntl.h>
    #include <string.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <string>
#include <utility>
#include <stdexcept>
#include <memory>
#include <cassert>

#if defined(NES_WIN32_SHARED_MEMORY)

namespace nes
{

static constexpr const char* shared_memory_root = u8"Local\\";

enum class shared_memory_option : std::uint32_t
{
    none = 0x00,
    constant = 0x01
};

constexpr shared_memory_option operator&(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator&=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left & right;
    return left;
}

constexpr shared_memory_option operator|(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator|=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left | right;
    return left;
}

constexpr shared_memory_option operator^(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator^=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left ^ right;
    return left;
}

constexpr shared_memory_option operator~(shared_memory_option value) noexcept
{
    return static_cast<shared_memory_option>(~static_cast<std::uint32_t>(value));
}

namespace impl
{

inline std::uintptr_t get_allocation_granularity() noexcept
{
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwAllocationGranularity;
}

static const std::uintptr_t allocation_granularity_mask{~(get_allocation_granularity() - 1)};

}

template<typename T>
struct map_deleter
{
    void operator()(T* ptr) const noexcept
    {
        if(ptr)
            UnmapViewOfFile(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask));
    }
};

template<typename T>
using unique_map_t = std::unique_ptr<T, map_deleter<T>>;
template<typename T>
using shared_map_t = std::shared_ptr<T>;
template<typename T>
using weak_map_t = std::weak_ptr<T>;

struct raw_map_deleter
{
    void operator()(void* ptr) const noexcept
    {
        if(ptr)
            UnmapViewOfFile(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask));
    }
};

using unique_raw_map_t = std::unique_ptr<void, raw_map_deleter>;
using shared_raw_map_t = std::shared_ptr<void>;
using weak_raw_map_t = std::weak_ptr<void>;

class shared_memory
{
public:
    using native_handle_type = HANDLE;

public:
    constexpr shared_memory() noexcept = default;

    explicit shared_memory(const std::string& name, std::uint64_t size)
    {
        assert(!std::empty(name) && "nes::shared_memory::shared_memory called with empty name.");
        assert(size != 0 && "nes::shared_memory::shared_memory called with size == 0.");

        const auto native_name{to_wide(shared_memory_root + name)};

        m_handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), std::data(native_name));
        if(!m_handle || GetLastError() == ERROR_ALREADY_EXISTS)
            throw std::runtime_error{"Failed to create shared memory. " + get_error_message()};
    }

    explicit shared_memory(const std::string& name, shared_memory_option options = shared_memory_option::none)
    {
        assert(!std::empty(name) && "nes::shared_memory::shared_memory called with empty name.");

        const auto native_name{to_wide(shared_memory_root + name)};
        const DWORD access = static_cast<bool>(options & shared_memory_option::constant) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;

        m_handle = OpenFileMappingW(access, FALSE, std::data(native_name));
        if(!m_handle)
            throw std::runtime_error{"Failed to open shared memory. " + get_error_message()};
    }

    ~shared_memory()
    {
        if(m_handle)
            CloseHandle(m_handle);
    }

    shared_memory(const shared_memory&) = delete;
    shared_memory& operator=(const shared_memory&) = delete;

    shared_memory(shared_memory&& other) noexcept
    :m_handle{std::exchange(other.m_handle, nullptr)}
    {

    }

    shared_memory& operator=(shared_memory&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, nullptr);

        return *this;
    }

    template<typename T>
    unique_map_t<T> map(std::uint64_t offset, shared_memory_option options = (std::is_const<T>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        static_assert(std::is_trivial<T>::value, "Behaviour is undefined if T is not a trivial type.");
        assert(m_handle && "nes::shared_memory::map called with an invalid handle.");

        const DWORD access = static_cast<bool>(options & shared_memory_option::constant) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + sizeof(T))};

        auto* ptr{MapViewOfFile(m_handle, access, static_cast<DWORD>(aligned_offset >> 32), static_cast<DWORD>(aligned_offset), real_size)};
        if(!ptr)
            throw std::runtime_error{"Failed to map shared memory. " + get_error_message()};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_map_t<T>{static_cast<T*>(ptr)};
    }

    template<typename T>
    shared_map_t<T> shared_map(std::uint64_t offset, shared_memory_option options = (std::is_const<T>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        return shared_map_t<T>{map<T>(offset, options)};
    }

    unique_raw_map_t raw_map(std::uint64_t offset, std::size_t size, shared_memory_option options = shared_memory_option::none) const
    {
        assert(m_handle && "nes::shared_memory::raw_map called with an invalid handle.");

        const DWORD access = static_cast<bool>(options & shared_memory_option::constant) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + size)};

        void* ptr{MapViewOfFile(m_handle, access, static_cast<DWORD>(aligned_offset >> 32), static_cast<DWORD>(aligned_offset), real_size)};
        if(!ptr)
            throw std::runtime_error{"Failed to map shared memory. " + get_error_message()};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_raw_map_t{ptr};
    }

    shared_raw_map_t shared_raw_map(std::uint64_t offset, std::size_t size, shared_memory_option options = shared_memory_option::none) const
    {
        return shared_raw_map_t{raw_map(offset, size, options)};
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
        const auto required_size = MultiByteToWideChar(CP_UTF8, 0, std::data(path), std::size(path), nullptr, 0);
        out_path.resize(required_size);

        if(!MultiByteToWideChar(CP_UTF8, 0, std::data(path), std::size(path), std::data(out_path), std::size(out_path)))
            throw std::runtime_error{"Failed to convert the path to wide."};

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

private:
    native_handle_type m_handle{};
};

}

#elif defined(NES_POSIX_SHARED_MEMORY)


namespace nes
{

static constexpr const char* shared_memory_root = u8"/";

enum class shared_memory_option : std::uint32_t
{
    none = 0x00,
    constant = 0x01
};

constexpr shared_memory_option operator&(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator&=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left & right;
    return left;
}

constexpr shared_memory_option operator|(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator|=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left | right;
    return left;
}

constexpr shared_memory_option operator^(shared_memory_option left, shared_memory_option right) noexcept
{
    return static_cast<shared_memory_option>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}

constexpr shared_memory_option& operator^=(shared_memory_option& left, shared_memory_option right) noexcept
{
    left = left ^ right;
    return left;
}

constexpr shared_memory_option operator~(shared_memory_option value) noexcept
{
    return static_cast<shared_memory_option>(~static_cast<std::uint32_t>(value));
}

namespace impl
{

inline std::uintptr_t get_allocation_granularity() noexcept
{
    return static_cast<std::uintptr_t>(sysconf(_SC_PAGE_SIZE));
}

static const std::uintptr_t allocation_granularity_mask{~(get_allocation_granularity() - 1)};

}

template<typename T>
struct map_deleter
{
    void operator()(T* ptr) const noexcept
    {
        if(ptr)
            munmap(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask), sizeof(T));
    }
};

template<typename T>
using unique_map_t = std::unique_ptr<T, map_deleter<T>>;
template<typename T>
using shared_map_t = std::shared_ptr<T>;
template<typename T>
using weak_map_t = std::weak_ptr<T>;

struct raw_map_deleter
{
    void operator()(void* ptr) const noexcept
    {
        if(ptr)
            munmap(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask), size);
    }

    std::size_t size{};
};

using unique_raw_map_t = std::unique_ptr<void, raw_map_deleter>;
using shared_raw_map_t = std::shared_ptr<void>;
using weak_raw_map_t = std::weak_ptr<void>;

class shared_memory
{
public:
    using native_handle_type = int;

public:
    constexpr shared_memory() noexcept = default;

    explicit shared_memory(const std::string& name, std::uint64_t size)
    {
        assert(!std::empty(name) && "nes::shared_memory::shared_memory called with empty name.");
        assert(size != 0 && "nes::shared_memory::shared_memory called with size == 0.");

        const auto native_name{shared_memory_root + name};

        m_handle = shm_open(std::data(native_name), O_RDWR | O_CREAT | O_TRUNC, 0660);
        if(m_handle == -1)
            throw std::runtime_error{"Failed to create shared memory. " + std::string{strerror(errno)}};

        if(ftruncate(m_handle, static_cast<off_t>(size)) == -1)
        {
            close(m_handle);
            throw std::runtime_error{"Failed to set shared memory size. " + std::string{strerror(errno)}};
        }
    }

    explicit shared_memory(const std::string& name, shared_memory_option options = shared_memory_option::none)
    {
        assert(!std::empty(name) && "nes::shared_memory::shared_memory called with empty name.");

        const auto native_name{shared_memory_root + name};
        const auto access = static_cast<bool>(options & shared_memory_option::constant) ? O_RDONLY : O_RDWR;

        m_handle = shm_open(std::data(native_name), access, 0660);
        if(m_handle == -1)
            throw std::runtime_error{"Failed to open shared memory. " + std::string{strerror(errno)}};
    }

    ~shared_memory()
    {
        if(m_handle != -1)
            close(m_handle);
    }

    shared_memory(const shared_memory&) = delete;
    shared_memory& operator=(const shared_memory&) = delete;

    shared_memory(shared_memory&& other) noexcept
    :m_handle{std::exchange(other.m_handle, -1)}
    {

    }

    shared_memory& operator=(shared_memory&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, -1);

        return *this;
    }

    template<typename T>
    unique_map_t<T> map(std::uint64_t offset, shared_memory_option options = (std::is_const<T>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        static_assert(std::is_trivial<T>::value, "Behaviour is undefined if T is not a trivial type.");
        assert(m_handle != -1 && "nes::shared_memory::map called with an invalid handle.");

        const auto access = static_cast<bool>(options & shared_memory_option::constant) ? PROT_READ : PROT_READ | PROT_WRITE;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + sizeof(T))};

        auto* ptr{mmap(nullptr, real_size, access, MAP_SHARED, m_handle, static_cast<off_t>(aligned_offset))};
        if(ptr == MAP_FAILED)
            throw std::runtime_error{"Failed to map shared memory. " + std::string{strerror(errno)}};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_map_t<T>{static_cast<T*>(ptr)};
    }

    template<typename T>
    shared_map_t<T> shared_map(std::uint64_t offset, shared_memory_option options = (std::is_const<T>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        return shared_map_t<T>{map<T>(offset, options)};
    }

    unique_raw_map_t raw_map(std::uint64_t offset, std::size_t size, shared_memory_option options = shared_memory_option::none) const
    {
        assert(m_handle != -1 && "nes::shared_memory::raw_map called with an invalid handle.");

        const auto access = static_cast<bool>(options & shared_memory_option::constant) ? PROT_READ : PROT_READ | PROT_WRITE;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + size)};

        auto* ptr{mmap(nullptr, real_size, access, MAP_SHARED, m_handle, static_cast<off_t>(aligned_offset))};
        if(ptr == MAP_FAILED)
            throw std::runtime_error{"Failed to map shared memory. " + std::string{strerror(errno)}};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_raw_map_t{ptr, raw_map_deleter{real_size}};
    }

    shared_raw_map_t shared_raw_map(std::uint64_t offset, std::size_t size, shared_memory_option options = shared_memory_option::none) const
    {
        return shared_raw_map_t{raw_map(offset, size, options)};
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    native_handle_type m_handle{-1};
};

}

#endif

#endif
