#include "clever_set.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <random>
#include <ctime>

#define CATCH_CONFIG_NO_EXPERIMENTAL_STATIC_ANALYSIS_SUPPORT

#include <catch2/catch_test_macros.hpp>

#define MAKE_HASH(TYPE)                          \
    template <>                                  \
    struct std::hash<TYPE> {                     \
        size_t operator()(const TYPE& x) const { \
            return hasher(x.x);                  \
        }                                        \
        ::std::hash<int> hasher;                 \
    };

namespace {

struct FirstHashable {
    int x;

    bool operator==(const FirstHashable& r) const {
        return x == r.x;
    }
};

struct SecondHashable {
    int x;

    bool operator==(const SecondHashable& r) const {
        return x == r.x;
    }
    bool operator!=(const SecondHashable& r) const {
        return x != r.x;
    }
    bool operator<(const SecondHashable& r) const {
        return x < r.x;
    }
};

struct ThirdHashable {
    int x;
};

bool operator>(const ThirdHashable& a, const ThirdHashable& b) {
    return a.x > b.x;
}

struct FirstComparable {
    int x;

    bool operator<(const FirstComparable& r) const {
        return x < r.x;
    }
};

struct SecondComparable {
    int x;

    friend bool operator>(const SecondComparable& a, const SecondComparable& b) {
        return a.x > b.x;
    }
};

struct FirstBad {
    int x;
};

struct SecondBad {
    int x;

    bool operator==(const SecondBad& r) const {
        return x == r.x;
    }
};

struct ThirdBad {
    int x;
};

}  // namespace

MAKE_HASH(FirstHashable);
MAKE_HASH(SecondHashable);
MAKE_HASH(ThirdHashable);
MAKE_HASH(SecondComparable);
MAKE_HASH(ThirdBad);

namespace {

template <class T>
void TestCase() {
    CleverSet<T> s;
    s.Insert(T{1});
    REQUIRE(s.Find(T{1}));
    REQUIRE_FALSE(s.Insert(T{1}));
    REQUIRE(s.Insert(T{2}));
    REQUIRE(s.Erase(T{2}));
    REQUIRE_FALSE(s.Erase(T{0}));
}

template <class StdSet, class T>
void LongTest() {
    constexpr auto kN = 100'000;

    StdSet std_s;
    CleverSet<T> s;
    std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist{1, kN / 10};

    for (auto i = 0; i < kN; ++i) {
        auto key = T{dist(gen)};
        switch (dist(gen) % 6) {
            case 0:
                REQUIRE(s.Find(key) == (std_s.find(key) != std_s.end()));
                break;
            case 1:
                REQUIRE(s.Erase(key) == std_s.erase(key));
                break;
            default:
                REQUIRE(s.Insert(key) == std_s.insert(key).second);
        }
        REQUIRE(s.Size() == std_s.size());
    }
}

template <class T>
void AddressTest() {
    SECTION("Array") {
        T a[] = {{1}, {2}, {1}};
        CleverSet<T> s;

        REQUIRE(s.Insert(a[0]));
        REQUIRE(s.Insert(a[1]));
        REQUIRE_FALSE(s.Insert(a[0]));

        REQUIRE(s.Insert(a[2]));
        a[2] = T{3};
        REQUIRE_FALSE(s.Insert(a[2]));

        REQUIRE(s.Find(a[2]));
        REQUIRE(s.Erase(a[2]));
        REQUIRE_FALSE(s.Find(a[2]));
        REQUIRE(s.Size() == 2);
        REQUIRE_FALSE(s.Erase(a[2]));

        a[0] = T{2};
        REQUIRE(s.Find(a[0]));
        REQUIRE(s.Erase(a[0]));
        REQUIRE(s.Size() == 1);
    }

    SECTION("Vector") {
        CleverSet<T> s;
        std::vector<T> data(100, T{4});
        for (auto& x : data) {
            REQUIRE(s.Insert(x));
        }
        CHECK(s.Size() == data.size());
    }

    SECTION("OneElement") {
        CleverSet<T> s;
        T obj{5};
        REQUIRE(s.Insert(obj));
        for (auto i = 0; i < 10; ++i) {
            obj = T{i};
            REQUIRE_FALSE(s.Insert(obj));
        }
        CHECK(s.Size() == 1u);
    }
}

}  // namespace

TEST_CASE("Simple") {
    CleverSet<int> s;
    s.Insert(1);
    s.Insert(2);
    s.Insert(3);
    REQUIRE_FALSE(s.Find(4));
    REQUIRE(s.Find(1));
    REQUIRE(s.Erase(2));
}

TEST_CASE("Hashset") {
    TestCase<FirstHashable>();
    TestCase<SecondHashable>();
    LongTest<std::unordered_set<FirstHashable>, FirstHashable>();
}

TEST_CASE("Set") {
    TestCase<FirstComparable>();
    TestCase<SecondComparable>();
    TestCase<ThirdHashable>();
    LongTest<std::set<FirstComparable>, FirstComparable>();
}

TEST_CASE("Address") {
    AddressTest<FirstBad>();
    AddressTest<SecondBad>();
    AddressTest<ThirdBad>();
}
