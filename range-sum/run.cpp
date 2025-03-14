#include "range_sum.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Benchmark") {
    uint64_t result{};

    BENCHMARK("First") {
        result = RangeSum(0, 1'000'000'000);
    };
    CHECK(result == 499'999'999'500'000'000ull);

    BENCHMARK("Second") {
        result = RangeSum(12, 1'000'000'000, 3);
    };
    CHECK(result == 166'666'666'833'333'315ull);
}
