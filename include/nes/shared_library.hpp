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

#ifndef NOT_ENOUGH_STANDARDS_SHARED_LIBRARY
#define NOT_ENOUGH_STANDARDS_SHARED_LIBRARY

#if __has_include(<windows.h>)
    #define NES_WIN32_SHARED_LIBRARY
    #include <windows.h>
#elif __has_include(<unistd.h>)
    #define NES_POSIX_SHARED_LIBRARY
    #include <dlfcn.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <string>
#include <algorithm>
#include <cassert>
#include <stdexcept>

#if defined(NES_WIN32_SHARED_LIBRARY)

namespace nes
{

struct load_current_t{};
static constexpr load_current_t load_current{};

class shared_library
{
public:
    using native_handle_type = HINSTANCE;

public:
    constexpr shared_library() noexcept = default;

    explicit shared_library(load_current_t)
    {
        m_handle = GetModuleHandleW(nullptr);
        if(!m_handle)
            throw std::runtime_error{"Can not load current binary file. " + get_error_message()};
    }

    explicit shared_library(const std::string& path)
    {
        assert(!std::empty(path) && "nes::shared_library::shared_library called with empty path.");

        m_handle = LoadLibraryW(to_wide(path).c_str());
        if(!m_handle)
            throw std::runtime_error{"Can not load binary file \"" + path + "\". " + get_error_message()};
        m_need_free = true;
    }

    ~shared_library()
    {
        if(m_handle && m_need_free)
            FreeLibrary(m_handle);
    }

    shared_library(const shared_library&) = delete;
    shared_library& operator=(const shared_library&) = delete;

    shared_library(shared_library&& other) noexcept
    :m_handle{std::exchange(other.m_handle, native_handle_type{})}
    ,m_need_free{std::exchange(other.m_need_free, false)}
    {

    }

    shared_library& operator=(shared_library&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, native_handle_type{});
        m_need_free = std::exchange(other.m_need_free, false);

        return *this;
    }

    template<typename Func, typename = std::enable_if_t<std::is_pointer_v<Func> && std::is_function_v<std::remove_pointer_t<Func>>>>
    Func load(const std::string& symbol) const noexcept
    {
        assert(!std::empty(symbol) && "nes::shared_library::load called with an empty symbol name.");
        assert(m_handle && "nes::shared_library::load called with invalid handle.");

        return reinterpret_cast<Func>(reinterpret_cast<void*>(GetProcAddress(m_handle, std::data(symbol))));
    }

    template<typename Func, typename = std::enable_if_t<std::is_function_v<Func>>>
    Func* load(const std::string& symbol) const noexcept
    {
        return load<Func*>(symbol);
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

private:
    std::wstring to_wide(std::string path)
    {
        std::transform(std::begin(path), std::end(path), std::begin(path), [](char c){return c == '/' ? '\\' : c;});

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

    native_handle_type m_handle{};
    bool m_need_free{};
};

}

#elif defined(NES_POSIX_SHARED_LIBRARY)

namespace nes
{

struct load_current_t{};
static constexpr load_current_t load_current{};

class shared_library
{
public:
    using native_handle_type = void*;

public:
    constexpr shared_library() noexcept = default;

    explicit shared_library(load_current_t)
    {
        m_handle = dlopen(nullptr, RTLD_NOW);
        if(!m_handle)
            throw std::runtime_error{"Can not load current binary file. " + std::string{dlerror()}};
    }

    explicit shared_library(const std::string& path)
    {
        assert(!std::empty(path) && "nes::shared_library::shared_library called with empty path.");

        m_handle = dlopen(std::data(path), RTLD_NOW);
        if(!m_handle)
            throw std::runtime_error{"Can not load binary file \"" + path + "\". " + std::string{dlerror()}};
    }

    ~shared_library()
    {
        if(m_handle)
            dlclose(m_handle);
    }

    shared_library(const shared_library&) = delete;
    shared_library& operator=(const shared_library&) = delete;

    shared_library(shared_library&& other) noexcept
    :m_handle{std::exchange(other.m_handle, native_handle_type{})}
    {

    }

    shared_library& operator=(shared_library&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, native_handle_type{});
        return *this;
    }

    template<typename Func, typename = std::enable_if_t<std::is_pointer_v<Func> && std::is_function_v<std::remove_pointer_t<Func>>>>
    Func load(const std::string& symbol) const noexcept
    {
        assert(!std::empty(symbol) && "nes::shared_library::load called with an empty symbol name.");
        assert(m_handle && "nes::shared_library::load called with invalid handle.");

        return reinterpret_cast<Func>(dlsym(m_handle, std::data(symbol)));
    }

    template<typename Func, typename = std::enable_if_t<std::is_function_v<Func>>>
    Func* load(const std::string& symbol) const noexcept
    {
        return load<Func*>(symbol);
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
