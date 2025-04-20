#include "take.h"
#include "get.h"

#include <ranges>
#include <algorithm>
#include <list>
#include <vector>
#include <set>
#include <deque>
#include <list>
#include <forward_list>

#include <catch2/catch_test_macros.hpp>

void Check(const auto& expected, auto&& range, auto&&... adaptors) {
    auto&& result_range = (range | ... | adaptors);

    using ValueType = std::ranges::range_value_t<decltype(result_range)>;
    using ExpectedValueType = std::ranges::range_value_t<decltype(expected)>;
    STATIC_CHECK(std::is_same_v<ValueType, ExpectedValueType>);

    CHECK(std::ranges::equal(result_range, expected));
}

template <std::ranges::forward_range Container>
    requires std::is_same_v<typename Container::value_type, int>
void TestTake() {
    static constexpr auto kContainerSize = 5;
    static constexpr auto kIota = std::views::iota(0, kContainerSize);
    static constexpr auto kMaxTake = kContainerSize + 3;

    Container c{kIota.begin(), kIota.end()};
    for (auto i = 0; i < kMaxTake; ++i) {
        auto view = c | std::views::take(i);
        Check(view, Take(c, i));
        Check(view, Take(i)(c));
        Check(view, c, Take(i));

        Check(view, c, Take(i), Take(kMaxTake));
        Check(view, c, Take(kMaxTake), Take(i));
        Check(view, c, Take(i), Take(kMaxTake), Take(kMaxTake));
        Check(view, c, Take(kMaxTake), Take(i), Take(kMaxTake));
        Check(view, c, Take(kMaxTake), Take(kMaxTake), Take(i));

        if constexpr (std::ranges::sized_range<Container>) {
            auto size = std::min<size_t>(i, kContainerSize);
            CHECK(Take(c, i).size() == size);
        }
    }

    auto view = Take(c, 3);
    using View = decltype(view);
    using Iterator = std::ranges::iterator_t<View>;
    using ContainerIterator = std::ranges::iterator_t<Container>;

    STATIC_CHECK(std::is_same_v<Iterator, ContainerIterator>);
    STATIC_CHECK(std::ranges::sized_range<Container> == std::ranges::sized_range<View>);

    auto value = 10;
    for (auto it = c.begin(); auto& x : view) {
        x = value;
        CHECK(*it++ == value);
        value += 10;
    }
}

TEST_CASE("Take") {
    TestTake<std::vector<int>>();
    TestTake<std::deque<int>>();
    TestTake<std::list<int>>();
    TestTake<std::forward_list<int>>();
}

struct Address {
    std::string city;
    uint32_t postcode;

    bool operator==(const Address&) const = default;
};

struct User {
    std::string name;
    int age;
    Address address;

    bool operator==(const User&) const = default;
};

TEST_CASE("Get iterator") {
    std::vector<User> users{
        {"Alice", 20, {"2", 125}}, {"Bob", 25, {"1", 100}}, {"Carol", 23, {"3", 130}}};

    auto names = Get<&User::name>(users);
    STATIC_REQUIRE(std::ranges::random_access_range<decltype(names)>);

    auto p_name0 = &users[0].name;
    auto p_name1 = &users[1].name;
    auto p_name2 = &users[2].name;

    auto it = names.begin();
    REQUIRE(&*it++ == p_name0);
    REQUIRE(&*it++ == p_name1);
    REQUIRE(&*it++ == p_name2);
    REQUIRE(it == names.end());

    it -= 2;
    REQUIRE(&*it == p_name1);
    it += 1;
    REQUIRE(&*it == p_name2);
    --++it;
    REQUIRE(&*it == p_name2);
    REQUIRE(&names.begin()[1] == p_name1);

    it += -2;
    REQUIRE(&*it == p_name0);
    it -= -2;
    REQUIRE(&*it == p_name2);

    auto it2 = it;
    REQUIRE(it == it2);
    REQUIRE(std::next(it) == std::next(it2));
    REQUIRE(std::next(it) == names.end());
    REQUIRE(std::prev(it) == std::prev(it2));
}

template <std::ranges::forward_range Container>
    requires std::is_same_v<typename Container::value_type, User>
void TestGet() {
    Container users{{"Alice", 20, {"2", 125}}, {"Bob", 25, {"1", 100}}, {"Carol", 23, {"3", 130}}};

    Check(std::array{20, 25, 23}, users | Get<&User::age>);
    Check(std::array{125u, 100u, 130u}, users, Get<&User::address>, Get<&Address::postcode>);

    auto ages = Get<&User::age>(users);
    auto postcodes = users | Get<&User::address> | Get<&Address::postcode>;

    using Iterator = decltype(postcodes.begin());
    STATIC_CHECK(std::forward_iterator<Iterator>);
    STATIC_CHECK(std::is_same_v<decltype(*ages.begin()), int&>);
    STATIC_CHECK(std::is_same_v<decltype(*postcodes.begin()), uint32_t&>);

    CHECK(std::ranges::max(ages) == 25);
    CHECK(std::ranges::min(postcodes) == 100);
    CHECK(&users.front().address.postcode == &postcodes.front());

    if constexpr (std::ranges::bidirectional_range<Container>) {
        STATIC_CHECK(std::bidirectional_iterator<Iterator>);
        CHECK(ages.back() == 23);
        CHECK(postcodes.back() == 130);

        auto names = users | Get<&User::name>;
        std::ranges::reverse(names);
        Check(std::vector<std::string>{"Carol", "Bob", "Alice"}, names);
    } else {
        STATIC_CHECK_FALSE(std::bidirectional_iterator<Iterator>);
    }

    constexpr auto kIsSized = std::ranges::sized_range<Container>;
    STATIC_CHECK(std::ranges::sized_range<decltype(ages)> == kIsSized);
    STATIC_CHECK(std::ranges::sized_range<decltype(postcodes)> == kIsSized);
    if constexpr (kIsSized) {
        CHECK(ages.size() == users.size());
        CHECK(postcodes.size() == users.size());
    }

    if constexpr (std::ranges::random_access_range<Container>) {
        STATIC_CHECK(std::random_access_iterator<Iterator>);

        // weird sorts
        std::ranges::sort(ages);
        std::ranges::sort(users | Get<&User::address> | Get<&Address::city>);

        Container expected{
            {"Carol", 20, {"1", 125}}, {"Bob", 23, {"2", 100}}, {"Alice", 25, {"3", 130}}};
        Check(expected, users);
    } else {
        STATIC_CHECK_FALSE(std::random_access_iterator<Iterator>);
    }
}

TEST_CASE("Get") {
    TestGet<std::vector<User>>();
    TestGet<std::deque<User>>();
    TestGet<std::list<User>>();
    TestGet<std::forward_list<User>>();
}
