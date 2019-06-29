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

#if defined(_WIN32)
    #define NES_WIN32_SHARED_MEMORY
    #include <windows.h>
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
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

template<class T>
struct is_unbounded_array: std::false_type {};
template<class T>
struct is_unbounded_array<T[]> : std::true_type {};

template<class T>
struct is_bounded_array: std::false_type {};
template<class T, std::size_t N>
struct is_bounded_array<T[N]> : std::true_type {};

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
struct map_deleter<T[]>
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
        static_assert(!impl::is_unbounded_array<T>::value, "T can not be an unbounded array type, i.e. T[]. Specify the size, or use the second overload if you don't know it at compile-time");
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

    template<typename T, typename ValueType = typename std::remove_extent<T>::type>
    unique_map_t<T> map(std::uint64_t offset, std::size_t count, shared_memory_option options = (std::is_const<ValueType>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        static_assert(!impl::is_bounded_array<T>::value, "T is an statically sized array, use the other overload of map instead of this one (remove the second parameter).");
        static_assert(impl::is_unbounded_array<T>::value, "T must be an array type, i.e. T[].");
        assert(m_handle && "nes::shared_memory::map called with an invalid handle.");

        const DWORD access = static_cast<bool>(options & shared_memory_option::constant) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + (sizeof(ValueType) * count))};

        auto* ptr{MapViewOfFile(m_handle, access, static_cast<DWORD>(aligned_offset >> 32), static_cast<DWORD>(aligned_offset), real_size)};
        if(!ptr)
            throw std::runtime_error{"Failed to map shared memory. " + get_error_message()};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_map_t<T>{static_cast<ValueType*>(ptr)};
    }

    template<typename T>
    shared_map_t<T> shared_map(std::uint64_t offset, std::size_t count, shared_memory_option options = (std::is_const<ValueType>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        return shared_map_t<T>{map<T>(offset, count, options)};
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

template<class T>
struct is_unbounded_array: std::false_type {};
template<class T>
struct is_unbounded_array<T[]> : std::true_type {};

template<class T>
struct is_bounded_array: std::false_type {};
template<class T, std::size_t N>
struct is_bounded_array<T[N]> : std::true_type {};

}

template<typename T>
struct map_deleter
{
    void operator()(T* ptr) const noexcept
    {
        if(ptr)
        {
            const auto base_address{reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask};

            munmap(reinterpret_cast<void*>(base_address), static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr) - base_address) + sizeof(T));
        }
    }
};

template<typename T>
struct map_deleter<T[]>
{
    void operator()(T* ptr) const noexcept
    {
        if(ptr)
        {
            const auto base_address{reinterpret_cast<std::uintptr_t>(ptr) & impl::allocation_granularity_mask};

            munmap(reinterpret_cast<void*>(base_address), static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr) - base_address) + (sizeof(T) * count));
        }
    }

    std::size_t count{};
};

template<typename T>
using unique_map_t = std::unique_ptr<T, map_deleter<T>>;
template<typename T>
using shared_map_t = std::shared_ptr<T>;
template<typename T>
using weak_map_t = std::weak_ptr<T>;

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
        static_assert(!impl::is_unbounded_array<T>::value, "T can not be an unbounded array type, i.e. T[]. Specify the size, or use the second overload if you don't know it at compile-time");
        assert(m_handle != -1 && "nes::shared_memory::map called with an invalid handle.");

        const auto access = static_cast<bool>(options & shared_memory_option::constant) ? PROT_READ : PROT_READ | PROT_WRITE;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + sizeof(T))};

        auto* ptr{mmap(nullptr, real_size, access, MAP_SHARED, m_handle, static_cast<off_t>(aligned_offset))};
        if(ptr == MAP_FAILED)
            throw std::runtime_error{"Failed to map shared memory. " + std::string{strerror(errno)}};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_map_t<T>{reinterpret_cast<T*>(ptr)};
    }

    template<typename T>
    shared_map_t<T> shared_map(std::uint64_t offset, shared_memory_option options = (std::is_const<T>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        return shared_map_t<T>{map<T>(offset, options)};
    }

    template<typename T, typename ValueType = typename std::remove_extent<T>::type>
    unique_map_t<T> map(std::uint64_t offset, std::size_t count, shared_memory_option options = (std::is_const<ValueType>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        static_assert(!impl::is_bounded_array<T>::value, "T is an statically sized array, use the other overload of map instead of this one (remove the second parameter).");
        static_assert(impl::is_unbounded_array<T>::value, "T must be an array type, i.e. T[].");
        assert(m_handle != -1 && "nes::shared_memory::map called with an invalid handle.");

        const auto access = static_cast<bool>(options & shared_memory_option::constant) ? PROT_READ : PROT_READ | PROT_WRITE;
        const auto aligned_offset{offset & impl::allocation_granularity_mask};
        const auto real_size{static_cast<std::size_t>((offset - aligned_offset) + (sizeof(ValueType) * count))};

        auto* ptr{mmap(nullptr, real_size, access, MAP_SHARED, m_handle, static_cast<off_t>(aligned_offset))};
        if(ptr == MAP_FAILED)
            throw std::runtime_error{"Failed to map shared memory. " + std::string{strerror(errno)}};

        ptr = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + (offset - aligned_offset));

        return unique_map_t<T>{reinterpret_cast<ValueType*>(ptr), map_deleter<T>{count}};
    }

    template<typename T, typename ValueType = typename std::remove_extent<T>::type>
    shared_map_t<T> shared_map(std::uint64_t offset, std::size_t count, shared_memory_option options = (std::is_const<ValueType>::value ? shared_memory_option::constant : shared_memory_option::none)) const
    {
        return shared_map_t<T>{map<T>(offset, count, options)};
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
