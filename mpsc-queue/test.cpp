#include "mpsc_queue.h"

#include <algorithm>
#include <thread>
#include <ranges>
#include <map>
#include <tuple>
#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

using Catch::Matchers::RangeEquals;

TEST_CASE("PushPop") {
    MPSCQueue<int> queue;
    queue.Push(1);
    queue.Push(2);

    auto x = queue.Pop();
    REQUIRE(x == 2);

    x = queue.Pop();
    REQUIRE(x == 1);

    x = queue.Pop();
    REQUIRE_FALSE(x.has_value());
}

TEST_CASE("SingleThread") {
    constexpr auto kRange = std::views::iota(0, 1024);

    MPSCQueue<int> queue;
    for (auto i : kRange) {
        queue.Push(i);
    }

    std::vector<int> dequeued;
    queue.DequeueAll([&](int value) { dequeued.push_back(value); });
    REQUIRE_THAT(dequeued, RangeEquals(kRange | std::views::reverse));
}

TEST_CASE("Destructor") {
    MPSCQueue<int> queue;
    queue.Push(1);
    queue.Push(2);
    queue.Push(3);
}

TEST_CASE("MoveOnly") {
    MPSCQueue<std::unique_ptr<std::string>> queue;
    queue.Push(std::make_unique<std::string>("aba"));
    queue.Push(std::make_unique<std::string>("caba"));

    auto x = queue.Pop();
    REQUIRE(x.has_value());
    REQUIRE(**x == "caba");

    queue.Push(std::make_unique<std::string>("foo"));
    auto n = 0;
    auto check = [&](std::unique_ptr<std::string> s) {
        REQUIRE(n < 2);
        if (n == 0) {
            REQUIRE(*s == "foo");
        } else {
            REQUIRE(*s == "aba");
        }
        ++n;
    };

    queue.DequeueAll(check);
    CHECK(n == 2);
}

TEST_CASE("Threaded") {
    constexpr auto kN = 100'000;
    constexpr auto kRange = std::views::iota(0, kN);
    constexpr auto kNumProducers = 8;

    MPSCQueue<int> queue;
    std::map<int, size_t> dequeued;

    std::jthread consumer{[&](std::stop_token token) {
        auto save = [&](int value) { ++dequeued[value]; };
        while (!token.stop_requested()) {
            queue.DequeueAll(save);
        }
        queue.DequeueAll(save);
    }};

    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumProducers; ++i) {
        threads.emplace_back([&] {
            for (auto j : kRange) {
                queue.Push(j);
            }
        });
    }

    threads.clear();
    consumer.request_stop();
    consumer.join();
    REQUIRE_THAT(dequeued | std::views::keys, RangeEquals(kRange));
    REQUIRE(std::ranges::count(dequeued | std::views::values, kNumProducers) == kN);
}
