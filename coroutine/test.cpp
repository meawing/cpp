#include "coroutine.h"

#include <random>
#include <ranges>
#include <algorithm>
#include <thread>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TimePoint Now() {
    return std::chrono::system_clock::now();
}

Coroutine SimpleTask(int& x) {
    x = 1;
    co_await 0s;
    x = 2;
    co_await 0s;
    x = 3;
}

TEST_CASE("Simple") {
    Scheduler scheduler;
    auto x = 0;

    scheduler.AddTask(SimpleTask, x);
    REQUIRE(x == 0);

    REQUIRE(scheduler.Step());
    REQUIRE(x == 1);

    REQUIRE(scheduler.Step());
    REQUIRE(x == 2);

    REQUIRE(scheduler.Step());
    REQUIRE(x == 3);

    REQUIRE_FALSE(scheduler.Step());
}

Coroutine TaskA(std::vector<int>& v) {
    v.push_back(1);
    co_await 100ms;
    v.push_back(2);
    co_await 100ms;
    v.push_back(3);
}

Coroutine TaskB(std::vector<int>& v) {
    v.push_back(4);
    co_await 40ms;
    v.push_back(5);
    co_await 140ms;
    v.push_back(6);
}

TEST_CASE("Two tasks") {
    std::vector<int> v;

    auto start = Now();
    {
        Scheduler scheduler;
        scheduler.AddTask(TaskA, v);
        scheduler.AddTask(TaskB, v);
        for (auto i = 0; i < 5; ++i) {
            REQUIRE(scheduler.Step());
        }
    }

    auto duration = Now() - start;
    CHECK(duration > 170ms);
    CHECK(duration < 190ms);
    CHECK(v == std::vector{1, 4, 5, 2, 6});
}

TEST_CASE("Many waits") {
    constexpr auto kNumCoroutines = 1'000;
    static constexpr auto kWaitTime = 100ms;
    constexpr auto kWaitTask = [](int& count, int index, TimePoint time_point) -> Coroutine {
        CHECK(Now() + kWaitTime / 2 < time_point);

        co_await time_point;
        CHECK(Now() >= time_point);
        CHECK(count++ == index);
    };

    Scheduler scheduler;
    auto count = 0;
    auto time_point = Now() + kWaitTime;

    for (auto i : std::views::iota(0, kNumCoroutines)) {
        scheduler.AddTask(kWaitTask, count, i, time_point);
    }
    REQUIRE(count == 0);

    scheduler.Run();
    REQUIRE_FALSE(scheduler.Step());

    CHECK(count == kNumCoroutines);
    auto now = Now();
    CHECK(now >= time_point);
    CHECK(now < time_point + kWaitTime / 2);
}

TEST_CASE("Order") {
    static constexpr auto kNumCoroutines = 100;

    auto start = Now();
    auto x = 0;
    Scheduler scheduler;
    for (auto i = 0; i < kNumCoroutines; ++i) {
        scheduler.AddTask(
            [](int& x, int index) -> Coroutine {
                static auto start = Now();

                CHECK(x++ == index);

                co_await (start + 10ms);
                CHECK(x++ == kNumCoroutines + index);

                co_await (start + 20ms);
                CHECK(x++ == 2 * kNumCoroutines + index);
            },
            x, i);
    }
    scheduler.Run();

    auto duration = Now() - start;
    CHECK(x == 3 * kNumCoroutines);
    CHECK(duration >= 20ms);
    CHECK(duration < 30ms);
}

Coroutine OrderTask(int& num_finished, int start_index, int wait_index) {
    static auto start = Now();
    static auto num_started = 0;

    CHECK(num_started++ == start_index);

    co_await (start + std::chrono::nanoseconds{wait_index});
    CHECK(num_finished++ == wait_index);
}

TEST_CASE("OrderTwo") {
    static constexpr auto kNumCoroutines = 100'000;
    static constexpr auto kRange = std::views::iota(0, kNumCoroutines);

    std::vector v(kRange.begin(), kRange.end());
    std::ranges::shuffle(v, std::mt19937{12});

    auto num_finished = 0;
    Scheduler scheduler;
    for (auto [i, wait_index] : std::views::zip(kRange, v)) {
        scheduler.AddTask(OrderTask, num_finished, i, wait_index);
    }
    scheduler.Run();

    CHECK(num_finished == kNumCoroutines);
}

Coroutine RecursiveTask(Scheduler& scheduler, int depth, int& count) {
    if (depth == 0) {
        ++count;
        co_return;
    }

    static auto start = Now();
    auto time_point = start + std::chrono::nanoseconds{depth};
    auto subtree_size = (1 << depth) - 1;

    auto old_count = count;
    scheduler.AddTask(RecursiveTask, scheduler, depth - 1, count);
    co_await time_point;
    CHECK(count == old_count + subtree_size);

    old_count = count;
    scheduler.AddTask(RecursiveTask, scheduler, depth - 1, count);
    co_await time_point;
    CHECK(count == old_count + subtree_size);

    ++count;
}

TEST_CASE("Recursive") {
    constexpr auto kDepth = 13;
    constexpr auto kTreeSize = (1 << (kDepth + 1)) - 1;

    Scheduler scheduler;
    auto count = 0;

    scheduler.AddTask(RecursiveTask, scheduler, kDepth, count);
    scheduler.Run();

    CHECK(count == kTreeSize);
}

Coroutine StressTask(int& count, int& num_started) {
    static auto start = Now();
    static std::mt19937 gen{42};
    static std::uniform_int_distribution dist{0, 1'000'000};

    ++num_started;
    while (true) {
        ++count;
        co_await (start + std::chrono::nanoseconds{dist(gen)});
    }
}

TEST_CASE("Stress") {
    static constexpr auto kNumCoroutines = 100'000;
    static constexpr auto kNumIterations = 1'000'000;

    Scheduler scheduler;
    auto count = 0;
    auto num_started = 0;
    for (auto i = 0; i < kNumCoroutines; ++i) {
        scheduler.AddTask(StressTask, count, num_started);
    }

    for (auto i = 0; i < kNumIterations; ++i) {
        scheduler.Step();
    }
    CHECK(num_started == kNumCoroutines);
    CHECK(count == kNumIterations);
}

Coroutine MultiThreadTask(std::thread::id& current_id) {
    std::unordered_set<std::thread::id> ids;

    while (true) {
        auto id = std::this_thread::get_id();
        CHECK(id == current_id);
        CHECK(ids.insert(id).second);
        co_await 0s;
    }
}

TEST_CASE("Threads") {
    auto id = std::this_thread::get_id();
    Scheduler scheduler;
    scheduler.AddTask(MultiThreadTask, id);
    CHECK(scheduler.Step());

    std::mutex mutex;
    std::vector<std::jthread> threads;
    for (auto i = 0; i < 100; ++i) {
        threads.emplace_back([&] {
            std::lock_guard guard{mutex};
            id = std::this_thread::get_id();
            CHECK(scheduler.Step());
        });
    }
}

class X {
public:
    void Run() {
        value_ = 0;

        Scheduler scheduler;
        scheduler.AddTask(std::mem_fn(&X::TaskA), this);
        scheduler.AddTask(std::mem_fn(&X::TaskB), this);
        scheduler.Run();

        CHECK(value_ == 5);
    }

private:
    Coroutine TaskA() {
        CHECK(value_++ == 0);

        co_await (time_point_ + 1ms);
        CHECK(value_++ == 2);

        co_await (time_point_ + 2ms);
        CHECK(value_++ == 3);
    }

    Coroutine TaskB() {
        CHECK(value_++ == 1);

        co_await (time_point_ + 2ms);
        CHECK(value_++ == 4);
    }

    const TimePoint time_point_ = Now();
    int value_;
};

TEST_CASE("MemberOfClass") {
    X{}.Run();
}
