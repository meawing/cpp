#include "timerqueue.h"

#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <random>
#include <ranges>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

namespace {

constexpr auto kNow = std::chrono::system_clock::now;

int Inc(std::atomic<int>* a) {
    return a->fetch_add(1, std::memory_order::relaxed);
}

}  // namespace

TEST_CASE("AddGet") {
    auto start = kNow();

    TimerQueue<int> queue;
    queue.Add(start + 1ms, 0);
    queue.Add(start + 10ms, 1);
    queue.Add(start + 5ms, 2);

    REQUIRE(queue.Pop() == 0);
    REQUIRE(queue.Pop() == 2);
    REQUIRE(queue.Pop() == 1);
}

TEST_CASE("Forwarding lvalue") {
    TimerQueue<std::vector<int>> queue;
    std::vector v = {1, 2, 3};
    queue.Add(kNow(), v);

    v.push_back(4);
    REQUIRE(v == std::vector{1, 2, 3, 4});
    v = queue.Pop();
    REQUIRE(v == std::vector{1, 2, 3});
}

TEST_CASE("Forwarding rvalue") {
    auto item = std::make_unique<int>(5);
    auto* p = item.get();

    TimerQueue<std::unique_ptr<int>> queue;
    queue.Add(kNow(), std::move(item));
    item = queue.Pop();

    REQUIRE(item.get() == p);
    REQUIRE(*item == 5);
}

TEST_CASE("Blocking") {
    auto start = kNow();
    TimerQueue<int> queue;
    queue.Add(start + 50ms, 0);
    queue.Pop();
    REQUIRE(kNow() >= start + 50ms);
}

TEST_CASE("ManyThreads") {
    auto start = kNow();
    TimerQueue<int> queue;

    std::atomic_flag finished;
    std::thread worker([&] {
        queue.Pop();
        finished.test_and_set();
    });
    std::this_thread::sleep_for(50ms);

    REQUIRE_FALSE(finished.test());
    queue.Add(start, 4);
    worker.join();
    REQUIRE(finished.test());
}

TEST_CASE("TimerReschedule") {
    auto start = kNow();
    TimerQueue<int> queue;
    queue.Add(start + 1s, 5);

    auto value = 0;
    std::thread worker([&] { value = queue.Pop(); });
    std::this_thread::sleep_for(10ms);

    queue.Add(start, 6);
    worker.join();
    REQUIRE(value == 6);
    REQUIRE(kNow() < start + 100ms);
}

TEST_CASE("NoReschedule") {
    auto start = kNow();
    TimerQueue<int> queue;
    queue.Add(start + 1s, 5);

    auto value = 0;
    std::thread worker([&] { value = queue.Pop(); });
    std::this_thread::sleep_for(10ms);

    queue.Add(start + 2s, 6);
    worker.join();
    REQUIRE(value == 5);
    auto diff = kNow() - start;
    REQUIRE(diff >= 1s);
    REQUIRE(diff < 2s);
}

TEST_CASE("Args") {
    struct X {
        X(int) {
        }
        X(double, std::string) {
        }
    };
    TimerQueue<X> queue;
    queue.Add(kNow(), 5);
    queue.Add(kNow(), 4.2, "hello");
    queue.Pop();
    queue.Pop();
}

TEST_CASE("Equal time") {
    const auto start = kNow();

    TimerQueue<int> queue;
    queue.Add(start + 10ms, 0);
    queue.Add(start + 10ms, 1);
    queue.Add(start + 20ms, 2);

    REQUIRE(queue.Pop() != 2);
    REQUIRE(queue.Pop() != 2);
}

TEST_CASE("Stress") {
    static constexpr auto kNumProducers = 2;
    static constexpr auto kNumConsumers = 10;
    static constexpr auto kNumElements = 300'000;

    std::atomic produced = 0;
    std::atomic consumed = 0;
    TimerQueue<int> queue;
    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumProducers; ++i) {
        threads.emplace_back([&] {
            for (auto i = Inc(&produced); i < kNumElements; i = Inc(&produced)) {
                queue.Add(kNow(), i);
            }
        });
    }
    std::vector<int> result(kNumElements);
    for (auto t = 0; t < kNumConsumers; ++t) {
        threads.emplace_back([&] {
            for (auto i = Inc(&consumed); i < kNumElements; i = Inc(&consumed)) {
                result[i] = queue.Pop();
            }
        });
    }
    threads.clear();

    std::ranges::sort(result);
    std::vector<int> answer(kNumElements);
    std::iota(answer.begin(), answer.end(), 0);
    REQUIRE(result == answer);
}
