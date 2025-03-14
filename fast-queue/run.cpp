#include "mpmc.h"
#include "runner.h"

#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

namespace {

using namespace std::chrono_literals;

void StressEnqueue(uint32_t num_producers) {
    MPMCBoundedQueue<int> queue{64};
    TimeRunner runner{1s};
    for (auto i = 0u; i < num_producers; ++i) {
        runner.Do([&] { queue.Enqueue(0); });
    }
    INFO(std::to_string(num_producers));
    CHECK(runner.Wait() < 10ns);
}

void StressEnqueueDequeue(uint32_t num_producers, uint32_t num_consumers) {
    MPMCBoundedQueue<int> queue{64};
    TimeRunner prod_runner{1s};
    for (auto i = 0u; i < num_producers; ++i) {
        prod_runner.Do([&] { queue.Enqueue(0); });
    }
    TimeRunner cons_runner{1s};
    for (auto i = 0u; i < num_consumers; ++i) {
        cons_runner.Do([&](int x) { queue.Dequeue(x); }, 0);
    }
    INFO(std::to_string(num_producers) + ' ' + std::to_string(num_consumers));
    CHECK(prod_runner.Wait() < 100ns);
    CHECK(cons_runner.Wait() < 100ns);
}

void CorrectnessEnqueueDequeue(uint32_t num_producers, uint32_t num_consumers) {
    std::vector<int> enqueued(num_producers);
    std::vector<int> dequeued(num_producers);
    MPMCBoundedQueue<int> queue{64};

    TimeRunner prod_runner{1s};
    for (auto i = 0u; i < num_producers; ++i) {
        auto c = std::make_shared<int>(0);
        auto func = [&, i, c] { *c += queue.Enqueue(i); };
        auto on_exit = [&, i, c] {
            std::atomic_ref<int>{enqueued[i]}.store(*c, std::memory_order_relaxed);
        };
        prod_runner.Do(TaskWithExit{std::move(func), std::move(on_exit)});
    }

    TimeRunner cons_runner{1s};
    for (auto i = 0u; i < num_consumers; ++i) {
        auto counters = std::make_shared<std::vector<int>>(num_producers);
        auto func = [&, counters, &c = *counters] {
            if (int value; queue.Dequeue(value)) {
                ++c[value];
            }
        };
        auto on_exit = [&, counters, &c = *counters] {
            for (auto i = 0u; i < num_producers; ++i) {
                std::atomic_ref<int>{dequeued[i]}.fetch_add(c[i], std::memory_order_relaxed);
            }
        };
        cons_runner.Do(TaskWithExit{std::move(func), std::move(on_exit)});
    }

    INFO(std::to_string(num_producers) + ' ' + std::to_string(num_consumers));
    CHECK(prod_runner.Wait() < 100ns);
    CHECK(cons_runner.Wait() < 100ns);
    int value;
    while (queue.Dequeue(value)) {
        std::atomic_ref<int>{dequeued[value]}.fetch_add(1, std::memory_order_relaxed);
    }
    CHECK_THAT(enqueued, Catch::Matchers::Equals(dequeued));
}

}  // namespace

TEST_CASE("Stress Enqueue") {
    for (auto num_threads : {1, 2, 4, 8}) {
        StressEnqueue(num_threads);
    }
}

TEST_CASE("Stress Enqueue Dequeue") {
    for (auto num_threads : {1, 2, 4}) {
        StressEnqueueDequeue(num_threads, num_threads);
    }
}

TEST_CASE("Correctness Enqueue Dequeue") {
    for (auto num_threads : {1, 2, 4}) {
        CorrectnessEnqueueDequeue(num_threads, num_threads);
    }
}
