#include "reduce.h"

#include <vector>
#include <ranges>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

using Catch::Generators::chunk;
using Catch::Generators::random;
using Catch::Generators::take;

TEST_CASE("Simple") {
    std::vector v = {1, 2, 3};
    CHECK(Reduce(v.begin(), v.end(), 0, std::plus{}) == 6);

    v = {};
    CHECK(Reduce(v.begin(), v.end(), 3, std::plus{}) == 3);

    v = {1};
    CHECK(Reduce(v.begin(), v.end(), 4, std::plus{}) == 5);

    v = {1, 2};
    CHECK(Reduce(v.begin(), v.end(), 5, std::multiplies{}) == 10);
}

class IntWrapper {
public:
    explicit IntWrapper(int v = 0) : v_{v + 42} {
    }
    IntWrapper operator+(const IntWrapper& other) const {
        return IntWrapper{v_ + other.v_};
    }
    int Value() const {
        return v_;
    }

private:
    int v_;
};

TEST_CASE("Neutral element") {
    std::vector<IntWrapper> v;
    for (auto i : std::views::iota(0, 1'000)) {
        IntWrapper init{i};
        auto answer = std::reduce(v.begin(), v.end(), init, std::plus{});
        auto result = Reduce(v.begin(), v.end(), init, std::plus{});
        REQUIRE(result.Value() == answer.Value());
        v.emplace_back(i);
    }
}

TEST_CASE("Random") {
    auto size = GENERATE(take(3'000, random(0u, 1'000u)));
    auto init = random(-100, 100).get();
    const auto v = chunk(size, random(-100'000, 100'000)).get();
    auto answer = std::reduce(v.begin(), v.end(), init, std::plus{});
    auto result = Reduce(v.begin(), v.end(), init, std::plus{});
    REQUIRE(result == answer);
}
