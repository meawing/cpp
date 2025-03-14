#include "integral_constant.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

using ps::SSizet;
using ps::literals::operator""_const;

#define STATIC_TYPE_ASSERT(val) static_assert(decltype(val)::value, #val);
#define STATIC_TYPE_ASSERT_FALSE(val) static_assert(!decltype(val)::value, #val);

TEST_CASE("Eq") {
    STATIC_TYPE_ASSERT(SSizet<1>{} == SSizet<1>{});
    STATIC_TYPE_ASSERT_FALSE(SSizet<2>{} == SSizet<1>{});
}

TEST_CASE("Neq") {
    STATIC_TYPE_ASSERT(SSizet<2>{} != SSizet<1>{});
    STATIC_TYPE_ASSERT_FALSE(SSizet<1>{} != SSizet<1>{});
}

TEST_CASE("Cmp") {
    STATIC_TYPE_ASSERT(SSizet<2>{} < SSizet<3>{});
    STATIC_TYPE_ASSERT_FALSE(SSizet<2>{} < SSizet<1>{});

    STATIC_TYPE_ASSERT(SSizet<4>{} > SSizet<3>{});
    STATIC_TYPE_ASSERT_FALSE(SSizet<2>{} > SSizet<3>{});
}

TEST_CASE("Literals") {
    STATIC_TYPE_ASSERT(SSizet<234>{} == 234_const);
    STATIC_TYPE_ASSERT(SSizet<2340>{} == 2340_const);
    STATIC_TYPE_ASSERT(SSizet<2340>{} == 2340_const);
}

TEST_CASE("Arithmetic") {
    static_assert(decltype(1_const)::kValue == 1);
    STATIC_TYPE_ASSERT(1_const + 2_const == 3_const);
    STATIC_TYPE_ASSERT(21_const * 2_const == 42_const);
    STATIC_TYPE_ASSERT(21_const - 2_const == 19_const);
    STATIC_TYPE_ASSERT(21_const / 2_const == 10_const);
}
