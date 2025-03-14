#include "mpsc_queue.h"
#include "runner.h"

#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

using namespace std::chrono_literals;

TEST_CASE("Pop") {
    MPSCQueue<int> queue;

    TimeRunner pop_runner{1200ms};
    pop_runner.Do([&] { queue.Pop(); });

    TimeRunner runner{1s};
    for (auto i = 0; i < 8; ++i) {
        runner.Do([&] { queue.Push(42); });
    }
    REQUIRE(runner.Wait() < 150ns);
}

TEST_CASE("DequeueAll") {
    MPSCQueue<int> queue;

    TimeRunner runner{1s};
    runner.Do([&] { queue.Push(24); });

    size_t count = 0;
    runner.Do([&] {
        std::this_thread::sleep_for(10ms);
        queue.DequeueAll([&](auto&&) { ++count; });
    });

    runner.Wait();
    CHECK(count > 5'000'000);
}
