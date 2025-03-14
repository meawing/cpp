#include "reduce.h"

#include <execution>
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

using Catch::Generators::chunk;
using Catch::Generators::random;

static int MinModule(int a, int b) {
    return std::abs(a) < std::abs(b) ? a : b;
};

TEST_CASE("Benchmark") {
    static constexpr auto kMax = 2'000'000'000;
    const auto v = chunk(100'000'000, random(-kMax, kMax)).get();
    int answer{}, answer_m{}, result{};

    BENCHMARK("Single thread") {
        answer = std::reduce(v.begin(), v.end(), kMax, MinModule);
    };
    BENCHMARK("Standart multithreading") {
        answer_m = std::reduce(std::execution::par, v.begin(), v.end(), kMax, MinModule);
    };
    REQUIRE(answer == answer_m);

    BENCHMARK("Multithreading") {
        result = Reduce(v.begin(), v.end(), kMax, MinModule);
    };
    REQUIRE(result == answer);
}
