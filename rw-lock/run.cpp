#include "rw_lock.h"
#include "runner.h"

#include <ranges>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

namespace {

constexpr auto kNumIterations = 1'000'000;

uint64_t MulMod(uint64_t a, uint64_t b, uint64_t mod) {
    uint64_t res = 0;
    while (a) {
        if (a & 1) {
            res += b;
            if (res >= mod) {
                res -= mod;
            }
        }
        a >>= 1;
        b <<= 1;
        if (b >= mod) {
            b -= mod;
        }
    }
    return res;
}

int64_t BinPow(int64_t a, int64_t b, int64_t mod) {
    if (!b) {
        return 1;
    } else if (b % 2) {
        return MulMod(a, BinPow(a, b - 1, mod), mod);
    } else {
        auto result = BinPow(a, b / 2, mod);
        return MulMod(result, result, mod);
    }
}

uint64_t RunBenchmark(uint32_t num_threads, uint32_t num_readers, uint64_t counter = 0) {
    static constexpr auto kPrime = 100'000'000'000'000'003ll;
    RWLock rw_lock;
    Runner runner{kNumIterations};
    for (auto i = 0u; i < num_readers; ++i) {
        runner.Do([&] {
            rw_lock.Read([&] {
                if (counter && (BinPow(counter, kPrime - 1, kPrime) != 1)) {
                    throw std::logic_error{"Fermat's little theorem is wrong?"};
                }
            });
        });
    }
    for (auto i = num_readers; i < num_threads; ++i) {
        runner.Do([&] { rw_lock.Write([&] { ++counter; }); });
    }
    runner.Wait();
    return counter;
}

}  // namespace

TEST_CASE("Benchmark") {
    auto num_threads = std::max(4u, std::thread::hardware_concurrency());
    uint64_t counter{};

    static constexpr auto kInitCounter = 67;
    BENCHMARK("ReadOnly") {
        counter = RunBenchmark(num_threads, num_threads, kInitCounter);
    };
    REQUIRE(counter == kInitCounter);

    BENCHMARK("WriteOnly") {
        counter = RunBenchmark(num_threads, 0);
    };
    REQUIRE(counter == kNumIterations);

    BENCHMARK("Half") {
        counter = RunBenchmark(num_threads, num_threads / 2);
    };
    REQUIRE(counter < kNumIterations);

    BENCHMARK("Reads") {
        counter = RunBenchmark(2, num_threads - 2, kInitCounter);
    };
    REQUIRE(counter < kNumIterations + kInitCounter);
}
