#include "constexpr_map.h"
#include "sort.h"

#include <array>
#include <ranges>
#include <string_view>
#include <vector>
#include <utility>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

constexpr auto MapInsert() {
    ConstexprMap<std::string_view, int> a;
    a["check"] = 2;
    a["x"] = 5;
    a["x"] = 3;
    a["4"] = 1;
    return a;
}

TEST_CASE("Insert") {
    constexpr auto kMap = MapInsert();
    STATIC_REQUIRE(kMap.Find("check"));
    STATIC_REQUIRE(kMap.Find("x"));
    STATIC_REQUIRE(kMap.Find("4"));
    STATIC_REQUIRE_FALSE(kMap.Find("aba"));
    STATIC_REQUIRE(kMap.Size() == 3u);
}

constexpr auto MapErase() {
    ConstexprMap<int, std::string_view, 2> a;
    a[17'239] = "5";
    a[-3] = "7";
    a.Erase(17'239);
    a[8] = "end";
    return a;
}

TEST_CASE("Erase") {
    constexpr auto kMap = MapErase();
    STATIC_REQUIRE(kMap.Find(-3));
    STATIC_REQUIRE(kMap[8] == "end");
    STATIC_REQUIRE_FALSE(kMap.Find(17'239));
    STATIC_REQUIRE(kMap.Size() == 2u);
}

TEST_CASE("Order") {
    {
        constexpr auto kMap = MapInsert();
        STATIC_REQUIRE(kMap.GetByIndex(0) == std::make_pair(std::string_view{"check"}, 2));
        STATIC_REQUIRE(kMap.GetByIndex(1) == std::make_pair(std::string_view{"x"}, 3));
        STATIC_REQUIRE(kMap.GetByIndex(2) == std::make_pair(std::string_view{"4"}, 1));
    }
    {
        constexpr auto kMap = MapErase();
        STATIC_REQUIRE(kMap.GetByIndex(0) == std::make_pair(-3, std::string_view{"7"}));
        STATIC_REQUIRE(kMap.GetByIndex(1) == std::make_pair(8, std::string_view{"end"}));
    }
}

TEST_CASE("Sort") {
    {
        constexpr auto kA = MapInsert();
        constexpr auto kMap = Sort(kA);
        STATIC_REQUIRE(kMap.GetByIndex(0) == std::make_pair(std::string_view{"4"}, 1));
        STATIC_REQUIRE(kMap.GetByIndex(1) == std::make_pair(std::string_view{"check"}, 2));
        STATIC_REQUIRE(kMap.GetByIndex(2) == std::make_pair(std::string_view{"x"}, 3));
    }
    {
        constexpr auto kA = MapErase();
        constexpr auto kMap = Sort(kA);
        STATIC_REQUIRE(kMap.GetByIndex(0) == std::make_pair(8, std::string_view{"end"}));
        STATIC_REQUIRE(kMap.GetByIndex(1) == std::make_pair(-3, std::string_view{"7"}));
    }
}

template <size_t n, int delta>
constexpr auto MakeMap() {
    constexpr auto kRange = std::views::iota(0, static_cast<int>(n));

    std::vector v(kRange.begin(), kRange.end());
    std::ranges::rotate(v, v.begin() + v.size() / 2);

    ConstexprMap<int, int, n> map;
    for (auto i : kRange) {
        map[v[i]] = v[i] + delta;
    }
    return map;
}

template <const auto& map, int delta, size_t... indexes>
constexpr void CheckMap(std::integer_sequence<size_t, indexes...>) {
    constexpr auto kCheck = []<size_t index>() constexpr {
        constexpr auto& kPair = map.GetByIndex(index);
        static_assert(kPair.first == map.Size() - index - 1);
        static_assert(kPair.second == kPair.first + delta);
        return 1;
    };
    constexpr auto kNum = (kCheck.template operator()<indexes>() + ...);
    static_assert(kNum == map.Size());
}

TEST_CASE("BigSort") {
    constexpr auto kN = 100;
    constexpr auto kDelta = 15;

    constexpr auto kA = MakeMap<kN, kDelta>();
    static constexpr auto kMap = Sort(kA);

    STATIC_CHECK(kA.Size() == kN);
    STATIC_CHECK(kMap.Size() == kN);
    CheckMap<kMap, kDelta>(std::make_index_sequence<kN>());
}
