#include "rw_spinlock.h"
#include "runner.h"
#include "util.h"

#include <atomic>
#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

using namespace std::chrono_literals;

static uint32_t Run(uint32_t num_readers, uint32_t num_writers, uint32_t num_iterations) {
    RWSpinLock lock;
    auto state = 0u;
    Runner runner{num_iterations};
    for (auto i = 0u; i < num_readers; ++i) {
        runner.Do([&, sum = 0u]() mutable {
            lock.LockRead();
            sum += state;
            lock.UnlockRead();
            return sum;
        });
    }
    for (auto i = 0u; i < num_writers; ++i) {
        runner.Do([&] {
            lock.LockWrite();
            ++state;
            lock.UnlockWrite();
        });
    }
    runner.Wait();
    return state;
}

TEST_CASE("Benchmark") {
    static constexpr auto kNumIterations = 3'000'000;
    for (auto num_threads : {1, 2, 4, 8}) {
        auto num_threads_str = std::to_string(num_threads);
        uint32_t state{};
        BENCHMARK("Reads:" + num_threads_str) {
            state = Run(num_threads, 0, kNumIterations);
        };
        REQUIRE(state == 0);
        BENCHMARK("Writes:" + num_threads_str) {
            state = Run(0, num_threads, kNumIterations);
        };
        REQUIRE(state == kNumIterations);
    }
    for (auto num_threads : {1, 2, 4}) {
        uint32_t state{};
        BENCHMARK("ReadsWrites:" + std::to_string(num_threads)) {
            state = Run(num_threads, num_threads, kNumIterations);
        };
        REQUIRE(state > 0);
        REQUIRE(state < kNumIterations);
    }
}

TEST_CASE("WithoutSleep") {
    static constexpr auto kThreadsCount = 4u;
    if (std::thread::hardware_concurrency() < kThreadsCount) {
        FAIL("hardware_concurrency < " << kThreadsCount);
    }
    size_t counter = 0u;
    RWSpinLock spin;
    TimeRunner runner{1s};
    CPUTimer timer;
    for (auto i = 0u; i < kThreadsCount; ++i) {
        runner.Do([&] {
            spin.LockWrite();
            ++counter;
            spin.UnlockWrite();
        });
    }
    runner.Wait();

    auto cpu_time = timer.GetTimes().cpu_time;
    CHECK(cpu_time > 400ms * kThreadsCount);
}
