#include "take.h"
#include "get.h"
#include "stride.h"

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

TEST_CASE("Stride iterator (step 2)") {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto s = Stride(v, 2);

    Check(std::array{1, 3, 5, 7, 9}, s);
    REQUIRE(!s.empty());
    REQUIRE(s.size() == 5);
    REQUIRE(s.front() == 1);
    REQUIRE(s.back() == 9);

    auto it = s.begin();
    STATIC_CHECK(std::random_access_iterator<decltype(it)>);

    REQUIRE(*(it + 3) == 7);
    it += 4;
    REQUIRE(*it == 9);
    REQUIRE(std::next(it) == s.end());

    REQUIRE(*(it - 3) == 3);
    it -= 2;
    REQUIRE(*it == 5);

    REQUIRE(s.end() - s.begin() == 5);
    REQUIRE(&s.begin()[3] == &v[6]);
    REQUIRE(s.begin() + 5 == s.end());
    REQUIRE(*(s.end() - 1) == 9);
    REQUIRE(*--s.end() == 9);
}

TEST_CASE("Stride iterator (step 3)") {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    auto s = v | Stride(3);

    Check(std::array{1, 4, 7, 10}, s);
    REQUIRE(!s.empty());
    REQUIRE(s.size() == 4);
    REQUIRE(s.front() == 1);
    REQUIRE(s.back() == 10);

    auto it = s.begin();
    STATIC_CHECK(std::random_access_iterator<decltype(it)>);

    REQUIRE(*(it + 2) == 7);
    it += 3;
    REQUIRE(*it == 10);
    REQUIRE(*(it + (-2)) == 4);
    REQUIRE(std::next(it) == s.end());

    REQUIRE(*(it - 2) == 4);
    it -= 3;
    REQUIRE(*it == 1);
    it -= -2;
    REQUIRE(*it == 7);
    it += -2;
    REQUIRE(*it == 1);

    REQUIRE(s.end() - s.begin() == 4);
    REQUIRE(&s.begin()[3] == &v[9]);
    REQUIRE(s.begin() + 4 == s.end());
    REQUIRE(*(s.end() - 1) == 10);
    REQUIRE(*--s.end() == 10);
}

template <std::ranges::bidirectional_range Container>
    requires std::is_same_v<typename Container::value_type, int> &&
             std::ranges::sized_range<Container>
void TestStride() {
    static constexpr auto kContainerSize = 1'000;
    static constexpr auto kIota = std::views::iota(0, kContainerSize);
    static constexpr auto kMaxStride = kContainerSize + 10;

    Container c{kIota.begin(), kIota.end()};
    Check(c, Stride(c, 1));
    Check(c, Stride(1)(c));
    Check(c, c, Stride(1));

    for (auto stride = 1; stride < kMaxStride; ++stride) {
        Container expected;
        for (auto i = 0; auto x : c) {
            if (i++ % stride == 0) {
                expected.push_back(x);
            }
        }
        Check(expected, c, Stride(stride));
        Check(expected | std::views::reverse, c, Stride(stride), std::views::reverse);
    }

    CHECK(std::ranges::equal(c | Stride(6), c | Stride(2) | Stride(3)));

    auto view1 = c | Stride(99);
    auto view2 = c | Stride(11) | Stride(9);
    CHECK(std::ranges::equal(view1, view2));

    auto rev1 = view1 | std::views::reverse;
    auto rev2 = view2 | std::views::reverse;
    CHECK(std::ranges::equal(rev1, rev2));

    STATIC_CHECK(std::ranges::bidirectional_range<decltype(view2)>);
    STATIC_CHECK(std::ranges::bidirectional_range<decltype(rev2)>);
    STATIC_CHECK(std::ranges::sized_range<decltype(view2)>);
    STATIC_CHECK(std::ranges::sized_range<decltype(rev2)>);

    constexpr auto kIsRandomAccess = std::ranges::random_access_range<Container>;
    STATIC_CHECK(std::ranges::random_access_range<decltype(view2)> == kIsRandomAccess);
    STATIC_CHECK(std::ranges::random_access_range<decltype(rev2)> == kIsRandomAccess);
}

TEST_CASE("Stride") {
    TestStride<std::vector<int>>();
    TestStride<std::deque<int>>();
    TestStride<std::list<int>>();
}

TEST_CASE("Get and Stride") {
    std::vector<User> users{
        {"Alice", 20, {"2", 125}}, {"Bob", 25, {"1", 100}}, {"Carol", 23, {"3", 130}}};

    auto view = users | Get<&User::address> | Stride(2) | Get<&Address::city>;
    STATIC_CHECK(std::ranges::random_access_range<decltype(view)>);
    Check(std::vector<std::string>{"2", "3"}, view);

    for (auto& a : view) {
        a = "test";
    }

    auto another = users | Stride(1) | Get<&User::address> | Get<&Address::city>;
    STATIC_CHECK(std::ranges::random_access_range<decltype(another)>);
    Check(std::vector<std::string>{"test", "1", "test"}, another);

    std::vector<Address> v(1'000'000);
    auto postcodes = v | Stride(10) | Get<&Address::postcode>;
    REQUIRE(postcodes.size() == 100'000);
    for (auto i = 0u; i < postcodes.size(); ++i) {
        postcodes[i] = i;
        CHECK(v[10 * i].postcode == i);
    }
}
