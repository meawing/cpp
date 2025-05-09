#pragma once

#include <cstddef>
#include <type_traits>
#include <concepts>

namespace ps {

template <size_t N>
struct SSizet {
    static constexpr size_t kValue = N;
};

// Arithmetic operators

template <size_t LHS, size_t RHS>
constexpr auto operator+(SSizet<LHS>, SSizet<RHS>) -> SSizet<LHS + RHS> {
    return {};
}

template <size_t LHS, size_t RHS>
constexpr auto operator-(SSizet<LHS>, SSizet<RHS>) -> SSizet<LHS - RHS> {
    return {};
}

template <size_t LHS, size_t RHS>
constexpr auto operator*(SSizet<LHS>, SSizet<RHS>) -> SSizet<LHS * RHS> {
    return {};
}

template <size_t LHS, size_t RHS>
constexpr auto operator/(SSizet<LHS>, SSizet<RHS>) -> SSizet<LHS / RHS> {
    return {};
}

// Comparison operators
// They return std::true_type if the relation holds, std::false_type otherwise.

template <size_t LHS, size_t RHS>
constexpr auto operator==(SSizet<LHS>, SSizet<RHS>)
    -> std::conditional_t<(LHS == RHS), std::true_type, std::false_type> {
    if constexpr (LHS == RHS) {
        return std::true_type{};
    } else {
        return std::false_type{};
    }
}

template <size_t LHS, size_t RHS>
constexpr auto operator!=(SSizet<LHS>, SSizet<RHS>)
    -> std::conditional_t<(LHS != RHS), std::true_type, std::false_type> {
    if constexpr (LHS != RHS) {
        return std::true_type{};
    } else {
        return std::false_type{};
    }
}

template <size_t LHS, size_t RHS>
constexpr auto operator<(SSizet<LHS>, SSizet<RHS>)
    -> std::conditional_t<(LHS < RHS), std::true_type, std::false_type> {
    if constexpr (LHS == RHS) {
        return std::true_type{};
    } else {
        return std::false_type{};
    }
}

template <size_t LHS, size_t RHS>
constexpr auto operator>(SSizet<LHS>, SSizet<RHS>)
    -> std::conditional_t<(LHS > RHS), std::true_type, std::false_type> {
    if constexpr (LHS > RHS) {
        return std::true_type{};
    } else {
        return std::false_type{};
    }
}

namespace literals {

template <char... C>
consteval auto operator"" _const() {
    constexpr auto kCompute = []() constexpr -> size_t {
        size_t result = 0;
        ((result = result * 10 + (C - '0')), ...);
        return result;
    };
    constexpr size_t kValue = kCompute();
    return SSizet<kValue>{};
}

}  // namespace literals
}  // namespace ps
