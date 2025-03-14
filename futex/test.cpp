#include "mutex.h"
#include "util.h"

#include <thread>
#include <atomic>
#include <ranges>
#include <cstring>
#include <chrono>
#include <vector>

#include <sys/resource.h>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("Simple") {
    Mutex mutex;
    auto x = 1;

    std::jthread first{[&] {
        mutex.Lock();
        CHECK(x == 1);
        x = 2;
        std::this_thread::sleep_for(1500ms);
        mutex.Unlock();
    }};
    std::this_thread::sleep_for(300ms);

    std::jthread second{[&] {
        auto start = std::chrono::steady_clock::now();
        mutex.Lock();
        CHECK(x == 2);
        x = 3;
        auto diff = std::chrono::steady_clock::now() - start;
        CHECK(diff > 1100ms);
        CHECK(diff < 1300ms);
        mutex.Unlock();
    }};
}

TEST_CASE("Counter") {
    static constexpr auto kNumIterations = 100'000;
    static constexpr auto kNumThreads = 8;

    auto counter = 0;
    Mutex mutex;

    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&] {
            for (auto j = 0; j < kNumIterations; ++j) {
                mutex.Lock();
                ++counter;
                mutex.Unlock();
            }
        });
    }
    threads.clear();

    CHECK(counter == kNumThreads * kNumIterations);
}

TEST_CASE("Spinlock") {
    Mutex mutex;
    std::atomic_flag holder_is_ready;

    std::jthread holder{[&] {
        mutex.Lock();
        holder_is_ready.test_and_set();
        holder_is_ready.notify_one();
        CPUTimer timer;
        std::this_thread::sleep_for(1s);

        auto cpu_time = timer.GetTimes().cpu_time;
        INFO("your threads do not sleep, you probably implemented spinlock");
        CHECK(cpu_time < 1ms);
        mutex.Unlock();
    }};
    holder_is_ready.wait(false);

    std::jthread waiter{[&] {
        CPUTimer timer{CPUTimer::THREAD};
        mutex.Lock();  // thread should not consume cpu until holder finished
        auto cpu_time = timer.GetTimes().cpu_time;
        CHECK(cpu_time < 1ms);
        mutex.Unlock();
    }};
}
