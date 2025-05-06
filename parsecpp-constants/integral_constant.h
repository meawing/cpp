#pragma once

#include <cstddef>
#include <type_traits>
#include <concepts>

namespace ps {
template <size_t N>
struct SSizet;


/*
    auto operator+(...)
    auto operator-(...)
    auto operator*(...)
    auto operator/(...)

    auto operator==(...)
    auto operator!=(...)
    auto operator<(...)
*/

namespace literals {
template<char ...C>
consteval auto operator ""_const();

}
}

