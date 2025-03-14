#include "concepts.h"

#include <functional>
#include <vector>
#include <string>
#include <list>
#include <set>
#include <map>
#include <bitset>
#include <deque>
#include <forward_list>
#include <variant>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Predicate") {
    auto simple = [](int) { return true; };

    struct Func {
        bool operator()(double) {
            return false;
        }
    };

    std::function<bool(std::vector<int>)> vector_pred = [&simple](const std::vector<int>&) {
        return static_cast<bool>(simple(1));
    };

    struct Int {
        int x;
    };

    STATIC_CHECK(Predicate<decltype(simple), int>);
    STATIC_CHECK(Predicate<Func, double>);
    STATIC_CHECK(Predicate<decltype(vector_pred), std::vector<int>>);
    STATIC_CHECK(!Predicate<Int, int>);
    STATIC_CHECK(!Predicate<std::hash<std::string>, std::string>);
    STATIC_CHECK(!Predicate<decltype(vector_pred), int>);
}

TEST_CASE("Indexable") {
    STATIC_CHECK(Indexable<std::vector<int>>);
    STATIC_CHECK(Indexable<std::map<int, std::string>>);
    STATIC_CHECK(!Indexable<std::map<std::string, int>>);
    STATIC_CHECK(!Indexable<std::list<int>>);
    STATIC_CHECK(Indexable<std::bitset<32>>);
    int* check[1];
    STATIC_CHECK(Indexable<decltype(check)>);

    struct MyWrapper {
        std::vector<std::string> names;

        std::string& operator[](size_t ind) {
            return names[ind];
        }
    };
    STATIC_CHECK(Indexable<MyWrapper>);
    STATIC_CHECK(!Indexable<int>);
    STATIC_CHECK(Indexable<int*>);

    struct Bad {
        std::vector<int> nums;

    private:
        int& operator[](size_t ind) {
            return nums[ind];
        }
    };

    STATIC_CHECK(!Indexable<Bad>);

    struct AnotherBad {
        void operator[](size_t) {
            // need to return something
        }
    };
    STATIC_CHECK(!Indexable<AnotherBad>);
    STATIC_CHECK(!Indexable<std::set<int>>);
}

struct X {};

struct Y {
    int a;
    double b;
};

struct MyStruct {
    operator std::string() {
        return "Hello";
    }
};

TEST_CASE("Serialize simple") {
    STATIC_CHECK(SerializableToJson<int>);
    STATIC_CHECK(SerializableToJson<double>);
    STATIC_CHECK(SerializableToJson<uint64_t>);
    STATIC_CHECK(SerializableToJson<size_t>);
    STATIC_CHECK(SerializableToJson<bool>);

    STATIC_CHECK(SerializableToJson<std::string>);
    STATIC_CHECK(SerializableToJson<std::string_view>);
    STATIC_CHECK(SerializableToJson<char*>);
    STATIC_CHECK(SerializableToJson<MyStruct>);

    STATIC_CHECK_FALSE(SerializableToJson<X>);
    STATIC_CHECK_FALSE(SerializableToJson<Y>);

    STATIC_CHECK_FALSE(SerializableToJson<std::pair<std::string, int>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::pair<std::string_view, bool>>);
}

TEST_CASE("Serialize optional") {
    STATIC_CHECK(SerializableToJson<std::optional<int>>);
    STATIC_CHECK(SerializableToJson<std::optional<std::string>>);
    STATIC_CHECK(SerializableToJson<std::optional<MyStruct>>);

    STATIC_CHECK_FALSE(SerializableToJson<std::optional<X>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::optional<Y>>);
}

template <template <class...> class C, class T, size_t N, bool answer>
void RecursiveCheck() {
    if constexpr (N == 0) {
        return;
    } else {
        STATIC_CHECK(SerializableToJson<T> == answer);
        RecursiveCheck<C, C<T>, N - 1, answer>();
        STATIC_CHECK(SerializableToJson<std::optional<T>> == answer);
        RecursiveCheck<C, C<std::optional<T>>, N - 1, answer>();
    }
}

template <template <class...> class C>
void CheckContainer() {
    STATIC_CHECK(SerializableToJson<C<int>>);
    STATIC_CHECK(SerializableToJson<C<std::string>>);
    STATIC_CHECK(SerializableToJson<C<C<bool>>>);
    STATIC_CHECK(SerializableToJson<C<C<C<float>>>>);

    RecursiveCheck<C, int, 3, true>();
    RecursiveCheck<C, MyStruct, 3, true>();
    RecursiveCheck<C, X, 3, false>();
    RecursiveCheck<C, Y, 3, false>();
}

TEST_CASE("Serialize array") {
    CheckContainer<std::vector>();
    CheckContainer<std::deque>();
    CheckContainer<std::list>();
    CheckContainer<std::forward_list>();

    STATIC_CHECK(SerializableToJson<std::vector<std::forward_list<int>>>);
    STATIC_CHECK(SerializableToJson<std::list<std::deque<MyStruct>>>);

    STATIC_CHECK(SerializableToJson<int[3]>);
    STATIC_CHECK(SerializableToJson<MyStruct[5]>);
    STATIC_CHECK(SerializableToJson<bool[7][9]>);
    STATIC_CHECK(SerializableToJson<std::vector<double[10]>>);
}

template <template <class...> class C, class T, size_t N, bool answer>
void RecursiveCheckMap() {
    if constexpr (N == 0) {
        return;
    } else {
        STATIC_CHECK(SerializableToJson<T> == answer);
        RecursiveCheckMap<C, C<std::string, T>, N - 1, answer>();
        STATIC_CHECK(SerializableToJson<std::optional<T>> == answer);
        RecursiveCheckMap<C, C<std::string, std::optional<T>>, N - 1, answer>();
        STATIC_CHECK(SerializableToJson<T> == answer);
        RecursiveCheckMap<C, C<std::string_view, T>, N - 1, answer>();
        STATIC_CHECK(SerializableToJson<std::optional<T>> == answer);
        RecursiveCheckMap<C, C<std::string_view, std::optional<T>>, N - 1, answer>();
    }
}

TEST_CASE("Serialize object") {
    STATIC_CHECK(SerializableToJson<std::map<std::string, int>>);
    STATIC_CHECK(SerializableToJson<std::map<const char*, std::vector<bool>>>);
    STATIC_CHECK(SerializableToJson<std::map<std::string, std::forward_list<MyStruct>>>);
    STATIC_CHECK(SerializableToJson<std::unordered_map<std::string, int>>);
    STATIC_CHECK(SerializableToJson<std::unordered_map<std::string_view, std::list<char>>>);

    STATIC_CHECK(SerializableToJson<std::vector<std::pair<std::string, MyStruct>>>);
    STATIC_CHECK(SerializableToJson<std::forward_list<std::pair<char*, float>>>);

    STATIC_CHECK_FALSE(SerializableToJson<std::map<std::string, std::optional<X>>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::map<int, int>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::map<bool, MyStruct>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::unordered_map<bool, bool>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::unordered_map<std::string, std::vector<X>>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::unordered_map<int, std::string>>);

    STATIC_CHECK_FALSE(SerializableToJson<std::deque<std::pair<std::string, X>>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::list<std::pair<char*, Y>>>);

    RecursiveCheckMap<std::map, MyStruct, 3, true>();
    RecursiveCheckMap<std::unordered_map, int, 3, true>();
    RecursiveCheckMap<std::map, X, 3, false>();
    RecursiveCheckMap<std::unordered_map, Y, 3, false>();
}

template <class T, size_t N, bool answer>
void RecursiveCheckVariant() {
    if constexpr (N == 0) {
        return;
    } else {
        STATIC_CHECK(SerializableToJson<T> == answer);
        RecursiveCheckVariant<std::variant<std::optional<T>, char, T>, N - 1, answer>();
        RecursiveCheckVariant<std::variant<T, std::vector<T>, bool, T, int>, N - 1, answer>();
    }
}

TEST_CASE("Serialize variant") {
    STATIC_CHECK(SerializableToJson<std::variant<int>>);
    STATIC_CHECK(SerializableToJson<std::variant<char, bool>>);
    STATIC_CHECK(SerializableToJson<std::variant<double, std::vector<float>>>);
    STATIC_CHECK(SerializableToJson<std::variant<bool, std::variant<char, float>>>);
    STATIC_CHECK(SerializableToJson<std::vector<std::variant<uint32_t, float>>>);
    STATIC_CHECK(SerializableToJson<std::unordered_map<std::string, std::variant<int, bool>>>);
    RecursiveCheckVariant<int, 4, true>();

    STATIC_CHECK_FALSE(SerializableToJson<std::variant<X>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::variant<Y>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::variant<int, X>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::variant<X, int>>);
    STATIC_CHECK_FALSE(SerializableToJson<std::list<std::variant<size_t, Y>>>);
    STATIC_CHECK_FALSE(
        SerializableToJson<std::map<std::string, std::variant<std::vector<X>, bool>>>);
    RecursiveCheckVariant<X, 4, false>();
}
