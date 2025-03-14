#include "string_constant.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

using ps::SString;
using ps::literals::operator""_cs;

#define STATIC_TYPE_ASSERT(val) static_assert(decltype(val)::value, #val);
#define STATIC_TYPE_ASSERT_FALSE(val) static_assert(!decltype(val)::value, #val);

TEST_CASE("Eq") {
    auto abc = SString<'a', 'b', 'c'>{};
    auto abd = SString<'a', 'b', 'd'>{};
    STATIC_TYPE_ASSERT(SString<'a'>{} == SString<'a'>{});
    STATIC_TYPE_ASSERT_FALSE(SString<'b'>{} == SString<'a'>{});

    STATIC_TYPE_ASSERT(abc == abc);
    STATIC_TYPE_ASSERT_FALSE(abd == abc);
}

TEST_CASE("Neq") {
    auto abc = SString<'a', 'b', 'c'>{};
    auto abd = SString<'a', 'b', 'd'>{};
    STATIC_TYPE_ASSERT_FALSE(SString<'a'>{} != SString<'a'>{});
    STATIC_TYPE_ASSERT(SString<'b'>{} != SString<'a'>{});

    STATIC_TYPE_ASSERT_FALSE(abc != abc);
    STATIC_TYPE_ASSERT(abd != abc);
}

TEST_CASE("Concat") {
    auto a = SString<'a'>{};
    auto bc = SString<'b', 'c'>{};
    auto abc0 = SString<'a', 'b', 'c'>{};
    auto abc1 = a + bc;
    STATIC_TYPE_ASSERT(abc0 == abc1);
}

TEST_CASE("Value") {
    auto abc = SString<'a', 'b', 'c'>{};
    static_assert(abc() == "abc");
    static_assert(abc.Size() == 3);
}

TEST_CASE("Literals") {
    auto abc0 = SString<'a', 'b', 'c'>{};
    auto abc1 = "abc"_cs;
    STATIC_TYPE_ASSERT(abc0 == abc1);
}
