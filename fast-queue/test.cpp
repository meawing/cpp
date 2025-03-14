#include "mpmc.h"

#include <thread>
#include <vector>
#include <atomic>
#include <ranges>
#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Enqueue") {
    MPMCBoundedQueue<int> queue{2};
    REQUIRE(queue.Enqueue(2));
    REQUIRE(queue.Enqueue(2));
    REQUIRE_FALSE(queue.Enqueue(2));
    REQUIRE_FALSE(queue.Enqueue(2));
}

TEST_CASE("Dequeue") {
    int val;
    MPMCBoundedQueue<int> queue{2};
    REQUIRE_FALSE(queue.Dequeue(val));
    REQUIRE_FALSE(queue.Dequeue(val));
}

TEST_CASE("EnqueueDequeue") {
    auto val = 0;
    MPMCBoundedQueue<int> queue{2};
    REQUIRE(queue.Enqueue(1));
    REQUIRE(queue.Dequeue(val));
    REQUIRE(val == 1);
    REQUIRE_FALSE(queue.Dequeue(val));

    REQUIRE(queue.Enqueue(2));
    REQUIRE(queue.Enqueue(3));
    REQUIRE_FALSE(queue.Enqueue(4));

    REQUIRE(queue.Dequeue(val));
    REQUIRE(val == 2);
    REQUIRE(queue.Dequeue(val));
    REQUIRE(val == 3);

    REQUIRE_FALSE(queue.Dequeue(val));
}

TEST_CASE("NoSpuriousFails") {
    static constexpr auto kRange = std::views::iota(0, 128 * 1024);
    static constexpr auto kNumThreads = 8;
    static constexpr auto kN = kNumThreads * kRange.size();
    MPMCBoundedQueue<int> queue{kN};
    std::atomic counter = 0;

    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&] {
            for (auto x : kRange) {
                counter.fetch_add(queue.Enqueue(x), std::memory_order_relaxed);
            }
        });
    }
    threads.clear();
    REQUIRE(counter == kN);

    for (auto i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&] {
            for (auto x : kRange) {
                counter -= queue.Dequeue(x);
            }
        });
    }
    threads.clear();
    REQUIRE(counter == 0);
}

TEST_CASE("NoQueueLock") {
    static constexpr auto kRange = std::views::iota(0, 100'000);
    static std::array<std::atomic<int>, kRange.size()> results;
    static constexpr auto kNumProducers = 4;
    static constexpr auto kNumConsumers = 4;
    MPMCBoundedQueue<int> queue{64};

    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumProducers; ++i) {
        threads.emplace_back([&] {
            for (auto x : kRange) {
                if (queue.Enqueue(x)) {
                    ++results[x];
                }
            }
        });
    }
    for (auto i = 0; i < kNumConsumers; ++i) {
        threads.emplace_back([&] {
            for (auto x : kRange) {
                if (queue.Dequeue(x)) {
                    --results[x];
                }
            }
        });
    }
    threads.clear();

    auto k = 0;
    while (queue.Dequeue(k)) {
        --results[k];
    }
    REQUIRE(std::ranges::all_of(results, [](const auto& a) { return a == 0; }));
    REQUIRE(queue.Enqueue(0));
    REQUIRE(queue.Dequeue(k));
    REQUIRE(k == 0);
}
