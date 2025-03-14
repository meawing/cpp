#include "semaphore.h"

#include <thread>
#include <vector>
#include <atomic>
#include <ranges>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

template <int concurrency_level>
void RunTest(int threads_count) {
    Semaphore semaphore{concurrency_level};
    auto time = 0;
    for (auto i : std::views::iota(0, concurrency_level)) {
        semaphore.Acquire([&time, i](int& count) {
            REQUIRE(time++ == i);
            REQUIRE(count-- == concurrency_level - i);
        });
    }
    REQUIRE(time == concurrency_level);

    std::vector<std::jthread> threads;
    time = 0;
    for (auto i : std::views::iota(0, threads_count)) {
        threads.emplace_back([&time, &semaphore, i] {
            semaphore.Acquire([&time, i](int& count) {
                REQUIRE(time++ == i);
                REQUIRE(count <= concurrency_level);
                REQUIRE(--count >= 0);
            });
            semaphore.Release();
        });
        std::this_thread::sleep_for(20ms);
    }
    REQUIRE(time == 0);

    for (auto i = 0; i < concurrency_level; ++i) {
        semaphore.Release();
    }
    threads.clear();
    REQUIRE(time == threads_count);
}

static void TestOrder() {
    Semaphore semaphore{1};
    auto time = 0;
    std::vector<std::jthread> threads;
    std::atomic_flag flag;

    threads.emplace_back([&] {
        semaphore.Acquire([&time](int& count) {
            REQUIRE(time++ == 0);
            REQUIRE(--count == 0);
        });
        flag.wait(false);
        semaphore.Release();
    });

    std::this_thread::sleep_for(20ms);

    threads.emplace_back([&] {
        semaphore.Acquire([&time](int& count) {
            REQUIRE(time++ == 1);
            REQUIRE(--count == 0);
        });
        semaphore.Release();
    });

    threads.emplace_back([&] {
        flag.wait(false);
        semaphore.Acquire([&time](int& count) {
            REQUIRE(time++ == 2);
            REQUIRE(--count == 0);
        });
        semaphore.Release();
    });

    std::this_thread::sleep_for(20ms);
    flag.test_and_set();
    flag.notify_all();
    threads.clear();
    REQUIRE(time == 3);
}

TEST_CASE("Mutex") {
    std::ranges::for_each(std::views::iota(0, 15), RunTest<1>);
}

TEST_CASE("Semaphore_3") {
    std::ranges::for_each(std::views::iota(0, 15), RunTest<3>);
}

TEST_CASE("Semaphore_5") {
    std::ranges::for_each(std::views::iota(0, 15), RunTest<5>);
}

TEST_CASE("Order") {
    for (auto i = 0; i < 5; ++i) {
        TestOrder();
    }
}
