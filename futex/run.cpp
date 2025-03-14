#include "mutex.h"
#include "runner.h"

#include <chrono>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

static void Run(uint32_t num_threads, std::chrono::steady_clock::duration time) {
    auto counter = 0;
    Mutex mutex;
    TimeRunner runner{1s};
    for (auto i = 0u; i < num_threads; ++i) {
        runner.Do([&] {
            mutex.Lock();
            ++counter;
            mutex.Unlock();
        });
    }
    INFO(std::to_string(num_threads));
    CHECK(runner.Wait() < time);
}

TEST_CASE("Benchmark") {
    Run(1, 30ns);
    for (auto num_threads : {2, 4, 8}) {
        Run(num_threads, 100ns);
    }
}
