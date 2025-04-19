#include "../hazard-ptr/hazard_ptr.h"
#include "stack.h"
#include "runner.h"

#include <array>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("Stress Push") {
    Stack<int> stack;
    TimeRunner runner{1s, 8};

    for (auto i = 0; i < 8; ++i) {
        TaskWithExit task{[&] { stack.Push(0); }, UnregisterThread};
        runner.DoWithInit(RegisterThread, std::move(task));
    }

    REQUIRE(runner.Wait() < 100ns);
}

static void StressPushPop(uint32_t num_push_threads, uint32_t num_pop_threads) {
    Stack<int> stack;

    TimeRunner push_runner{1s, num_push_threads};
    for (auto i = 0u; i < num_push_threads; ++i) {
        TaskWithExit task{[&] { stack.Push(0); }, UnregisterThread};
        push_runner.DoWithInit(RegisterThread, std::move(task));
    }

    TimeRunner pop_runner{1s, num_pop_threads};
    for (auto i = 0u; i < num_pop_threads; ++i) {
        auto func = [&, value = 0]() mutable { stack.Pop(&value); };
        TaskWithExit task{std::move(func), UnregisterThread};
        pop_runner.DoWithInit(RegisterThread, std::move(task));
    }

    INFO(std::to_string(num_push_threads) + ' ' + std::to_string(num_pop_threads));
    CHECK(push_runner.Wait() < 200ns);
    CHECK(pop_runner.Wait() < 15us);
}

TEST_CASE("Stress Push and Pop") {
    std::vector<std::pair<uint32_t, uint32_t>> tests = {{42, 8}, {75, 5}, {15, 5}, {50, 10}};
    for (auto [num_push_threads, num_pop_threads] : tests) {
        StressPushPop(num_push_threads, num_pop_threads);
    }
}
