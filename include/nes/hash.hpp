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
#include <cstring>
#include <array>
#include <memory>
#include <string>
#include <optional>
#include <variant>

namespace nes
{

template<typename WordT, std::size_t Size>
using hash_value_t = std::array<WordT, Size>;

template<typename WordT, std::size_t Size, typename T>
hash_value_t<WordT, Size> to_hash_value(const T& value) noexcept
{
    static_assert(std::is_standard_layout_v<T>, "The given type can not be used as a hash value");
    static_assert(sizeof(hash_value_t<WordT, Size>) == sizeof(T), "Types size does not match.");

    hash_value_t<WordT, Size> output{};
    std::memcpy(std::data(output), &value, sizeof(output));

    return output;
}

template<typename T, typename WordT, std::size_t Size>
T from_hash_value(const hash_value_t<WordT, Size>& value) noexcept
{
    static_assert(std::is_standard_layout_v<T>, "The given type can not be used as a hash value");

    T output{};
    std::memcpy(&output, std::data(value), sizeof(output));

    return output;
}

namespace hash_kernels
{

/*
[template<...>]
struct kernel_name
{
    using value_type = nes::hash_value_t<...>;

    value_type operator()(const std::uint8_t* data, std::size_t size) const [noexcept]
    {
        value_type hash_value{};

        //Compute hash from data

        return hash_value;
    }
};
*/

struct fnv_1a
{
    using value_type = hash_value_t<std::uint64_t, 1>;

    constexpr value_type operator()(const std::uint8_t* data, std::size_t size) const noexcept
    {
        constexpr std::uint64_t offset_basis{14695981039346656037ull};
        constexpr std::uint64_t prime{1099511628211ull};

        std::uint64_t value{offset_basis};
        for(std::size_t i{}; i < size; ++i)
        {
            value ^= data[i];
            value *= prime;
        }

        return to_hash_value<std::uint64_t, 1>(value);
    }
};

template<typename T, typename = void>
struct is_kernel : std::false_type{};
template<typename T>
struct is_kernel<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<T>().operator()(std::declval<const std::uint8_t*>(), std::declval<std::size_t>()))
    >
> : std::true_type{};
template<typename T>
inline constexpr bool is_kernel_v{is_kernel<T>::value};

template<typename Kernel>
struct is_noexcept : std::integral_constant<bool, noexcept(std::declval<Kernel>().operator()(std::declval<const std::uint8_t*>(), std::declval<std::size_t>()))> {};
template<typename Kernel>
inline constexpr bool is_noexcept_v{is_noexcept<Kernel>::value};

}

template<typename Kernel>
using kernel_hash_value_t = typename Kernel::value_type;

template<typename Kernel>
constexpr kernel_hash_value_t<Kernel> hash_combine(const kernel_hash_value_t<Kernel>& left, const kernel_hash_value_t<Kernel>& right) noexcept
{
    const std::pair pair{left, right};

    return Kernel{}(reinterpret_cast<const std::uint8_t*>(&pair), sizeof(pair));
}

template<typename T, typename Kernel = hash_kernels::fnv_1a, typename Enable = void>
struct hash;

template<typename T, typename Kernel>
struct hash<T, Kernel, std::enable_if_t<std::is_arithmetic_v<T>>>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(T value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(&value), sizeof(T));
    }
};

template<class T, typename Kernel>
struct hash<T*, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(T* value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(&value), sizeof(T*));
    }
};

template<typename CharT, typename Traits, typename Allocator, typename Kernel>
struct hash<std::basic_string<CharT, Traits, Allocator>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::basic_string<CharT, Traits, Allocator>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(std::data(value)), std::size(value) * sizeof(CharT));
    }
};

template<typename CharT, typename Traits, typename Kernel>
struct hash<std::basic_string_view<CharT, Traits>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::basic_string_view<CharT, Traits>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return Kernel{}(reinterpret_cast<const std::uint8_t*>(std::data(value)), std::size(value) * sizeof(CharT));
    }
};

template<typename T, typename Kernel>
struct hash<std::unique_ptr<T>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::unique_ptr<T>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return hash<typename std::unique_ptr<T>::pointer, Kernel>{}(value.get());
    }
};

template<typename T, typename Kernel>
struct hash<std::shared_ptr<T>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::shared_ptr<T>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        return hash<typename std::shared_ptr<T>::pointer, Kernel>{}(value.get());
    }
};

template<typename T, typename Kernel>
struct hash<std::optional<T>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::optional<T>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        if(value.has_value())
        {
            return hash<std::remove_const_t<T>, Kernel>{}(value.value());
        }

        return {};
    }
};

template<typename Kernel>
struct hash<std::monostate, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(std::monostate) const noexcept
    {
        return {};
    }
};

template<typename... Types, typename Kernel>
struct hash<std::variant<Types...>, Kernel>
{
    using value_type = kernel_hash_value_t<Kernel>;

    constexpr value_type operator()(const std::variant<Types...>& value) const noexcept(hash_kernels::is_noexcept_v<Kernel>)
    {
        if(value.valueless_by_exception())
        {
            return {};
        }
        else
        {
            const auto visitor = [](auto&& value) -> value_type
            {
                return hash<std::decay_t<decltype(value)>, Kernel>{}(value);
            };

            const value_type base{std::visit(visitor, value)};
            const value_type combine{hash<std::size_t, Kernel>{}(value.index())};

            return hash_combine<Kernel>(base, combine);
        }
    }
};

}

#endif
