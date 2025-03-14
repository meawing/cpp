#include "is_prime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using Catch::Generators::as;

TEST_CASE("Basic tests") {
    static constexpr auto kMul = 1'000'003ull;

    SECTION("Prime") {
        auto x = GENERATE(as<uint64_t>{}, 2, 3, 5, 7, 1021, 17239, kMul);
        INFO(x << " is prime");
        CHECK(IsPrime(x));
    }

    SECTION("Not prime") {
        auto x = GENERATE(as<uint64_t>{}, 0, 1, 4, 6, 16, 10002, kMul * kMul);
        INFO(x << " is not prime");
        CHECK_FALSE(IsPrime(x));
    }
}
