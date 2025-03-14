#include "commons.h"
#include "runner.h"
#include "concurrent_hash_map.h"

#include <thread>
#include <ranges>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

static constexpr auto kSeed = 82'944'584;

TEST_CASE("Benchmark") {
    const auto num_threads = GENERATE(2u, 4u, 8u);
    static constexpr auto kNumIterations = 1'000'000;

    ConcurrentHashMap<int, int> map(kNumIterations, num_threads);
    BENCHMARK("RandomInsertions:" + std::to_string(num_threads)) {
        Runner runner{kNumIterations};
        for (auto i : std::views::iota(0u, num_threads)) {
            Random rand{kSeed + 10 * i};
            runner.Do([&map, rand]() mutable { map.Insert(rand(), 1); });
        }
    };

    map.Clear();
    BENCHMARK("SpecialInsertions:" + std::to_string(num_threads)) {
        Runner runner{kNumIterations};
        EqualLowBits elb{16};
        runner.Do([&map, elb]() mutable { map.Insert(elb(), 1); });
        for (auto i : std::views::iota(1, static_cast<int>(num_threads))) {
            Increment inc{100'000 * i};
            runner.Do([&map, inc]() mutable { map.Insert(inc(), 1); });
        }
    };

    map.Clear();
    BENCHMARK("ManySearches:" + std::to_string(num_threads)) {
        Runner runner{kNumIterations};
        runner.Do([&map, rand = Random{kSeed - 1}]() mutable { map.Insert(rand(), 1); });
        for (auto i : std::views::iota(1u, num_threads)) {
            Random rand{kSeed - i - 1, 0, 1'000'000};
            runner.Do([&map, rand]() mutable { map.Find(rand()); });
        }
    };

    map.Clear();
    BENCHMARK("Deletions:" + std::to_string(num_threads)) {
        Runner runner{kNumIterations};
        for (auto i : std::views::iota(0u, num_threads)) {
            Random rand{kSeed + i / 2};
            if (i % 2) {
                runner.Do([&map, rand]() mutable { map.Insert(rand(), 1); });
            } else {
                runner.Do([&map, rand]() mutable { map.Erase(rand()); });
            }
        }
    };
}
