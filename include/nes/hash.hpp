///////////////////////////////////////////////////////////
/// Copyright 2020 Alexy Pellegrini
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

#ifndef NOT_ENOUGH_STANDARDS_HASH
#define NOT_ENOUGH_STANDARDS_HASH

#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <variant>

namespace nes
{

namespace hash_kernels
{

/*
struct kernel_name
{
    [constexpr] std::size_t operator()(const std::uint8_t* data, std::size_t size) const [noexcept]
    {
        std::size_t hash_value{};

        //Compute hash from data

        return hash_value;
    }
};
*/

struct fnv_1a
{
    constexpr std::size_t operator()(const std::uint8_t* data, std::size_t size) const noexcept
    {
        if constexpr(sizeof(std::size_t) == sizeof(std::uint32_t)) //Support for 32-bits
        {
            constexpr std::size_t offset_basis{2166136261u};
            constexpr std::size_t prime{16777619u};

            std::size_t value{offset_basis};
            for(std::size_t i{}; i < size; ++i)
            {
                value ^= data[i];
                value *= prime;
            }

            return value;
        }
        else
        {
            constexpr std::size_t offset_basis{14695981039346656037ull};
            constexpr std::size_t prime{1099511628211ull};

            std::size_t value{offset_basis};
            for(std::size_t i{}; i < size; ++i)
            {
                value ^= data[i];
                value *= prime;
            }

            return value;
        }
    }
};

}

constexpr std::size_t hash_combine(std::size_t left, std::size_t right) noexcept //See boost::hash_combine
{
    return left ^ (right + 0x9e3779b9u + (left << 6) + (left >> 2));
}

template<typename T, typename Kernel = hash_kernels::fnv_1a, typename Enable = void>
struct hash;

template<typename T, typename Kernel>
struct hash<T, Kernel, std::enable_if_t<std::is_arithmetic_v<T>>>
{
    constexpr std::size_t operator()(T value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(&value), sizeof(T));
    }
};

template<class T, typename Kernel>
struct hash<T*, Kernel>
{
    constexpr std::size_t operator()(T* value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(&value), sizeof(T*));
    }
};

template<typename CharT, typename Traits, typename Allocator, typename Kernel>
struct hash<std::basic_string<CharT, Traits, Allocator>, Kernel>
{
    std::size_t operator()(const std::basic_string<CharT, Traits, Allocator>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(std::data(value)), std::size(value) * sizeof(CharT));
    }
};

template<typename CharT, typename Traits, typename Kernel>
struct hash<std::basic_string_view<CharT, Traits>, Kernel>
{
    constexpr std::size_t operator()(const std::basic_string_view<CharT, Traits>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(std::data(value)), std::size(value) * sizeof(CharT));
    }
};

template<typename T, typename Kernel>
struct hash<std::unique_ptr<T>, Kernel>
{
    std::size_t operator()(const std::unique_ptr<T>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return hash<typename std::unique_ptr<T>::pointer, Kernel>{}(value.get());
    }
};

template<typename T, typename Kernel>
struct hash<std::shared_ptr<T>, Kernel>
{
    std::size_t operator()(const std::shared_ptr<T>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        return hash<typename std::shared_ptr<T>::pointer, Kernel>{}(value.get());
    }
};

template<typename T, typename Kernel>
struct hash<std::optional<T>, Kernel>
{
    constexpr std::size_t operator()(const std::optional<T>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        if(value.has_value())
        {
            return hash<std::remove_const_t<T>, Kernel>{}(value.value());
        }

        return 4000044773u;
    }
};

template<typename Kernel>
struct hash<std::monostate, Kernel>
{
    constexpr std::size_t operator()(std::monostate) const noexcept
    {
        return 4194968299u;
    }
};

template<typename... Types, typename Kernel>
struct hash<std::variant<Types...>, Kernel>
{
    constexpr std::size_t operator()(const std::variant<Types...>& value) const noexcept(noexcept(Kernel{}(std::declval<const std::uint8_t*>(), std::declval<std::size_t>())))
    {
        if(value.valueless_by_exception())
        {
            return 3194267473u;
        }
        else
        {
            const auto visitor = [](auto&& value) -> std::size_t
            {
                return hash<std::decay_t<decltype(value)>>{}(value);
            };

            const std::size_t base{std::visit(visitor, value)};
            const std::size_t combine{hash<std::size_t, Kernel>{}(value.index())};

            return hash_combine(base, combine);
        }
    }
};

}

#endif
