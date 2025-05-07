#pragma once

#include <string_view>
#include <utility>
#include <type_traits>

namespace ps {

template <char... s>
struct SString {
    // Returns the string view of the compile-time string
    static constexpr std::string_view operator()() noexcept {
        return std::string_view{kChars, sizeof...(s)};
    }

    // Returns the size of the string
    static constexpr size_t Size() noexcept {
        return sizeof...(s);
    }

private:
    // Helper array to store the character sequence at compile time
    static constexpr char kChars[sizeof...(s) + 1] = {s..., '\0'};
};

// Initialize the static character array
template <char... s>
constexpr char SString<s...>::kChars[sizeof...(s) + 1];

// Operator == for SString
template <char... s1, char... s2>
constexpr auto operator==(SString<s1...>, SString<s2...>) noexcept {
    return std::bool_constant<(std::string_view{SString<s1...>{}()} ==
                               std::string_view{SString<s2...>{}()})>{};
}

// Operator != for SString
template <char... s1, char... s2>
constexpr auto operator!=(SString<s1...> lhs, SString<s2...> rhs) noexcept {
    return std::bool_constant<!(lhs == rhs)>{};
}

// Concatenation operator + for SString
template <char... s1, char... s2>
constexpr auto operator+(SString<s1...>, SString<s2...>) noexcept {
    return SString<s1..., s2...>{};
}

// Literal operator for SString
namespace literals {
__extension__ template <typename CharT, CharT... s>
constexpr SString<s...> operator""_cs() noexcept {
    return {};
}
}  // namespace literals

}  // namespace ps
