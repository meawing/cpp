#include "is_prime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Benchmark") {
    std::pair<uint64_t, bool> tests[] = {{1000000000000000177ull, true},
                                         {1000000000375847551ull, true},
                                         {1000000016000000063ull, false},
                                         {3778117929678325589ull, false}};
    for (auto [x, is_prime] : tests) {
        INFO("Check " << x);
        bool result{};
        BENCHMARK(std::to_string(x)) {
            result = IsPrime(x);
        };
        CHECK(result == is_prime);
    }
}
