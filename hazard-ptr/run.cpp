#include "hazard_ptr.h"
#include "runner.h"

#include <thread>
#include <memory>
#include <chrono>
#include <atomic>

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace std::chrono_literals;

void RunReadOnly(uint32_t num_threads, std::atomic<int*>* value) {
    TimeRunner runner{1s};

    for (auto i = 0u; i < num_threads; ++i) {
        auto x = std::make_shared<int>(0);
        auto func = [&, x]() mutable {
            auto* ptr = Acquire(value);
            *x |= *ptr;
            Release();
        };
        TaskWithExit task{std::move(func), UnregisterThread};
        runner.DoWithInit(RegisterThread, std::move(task));
    }

    INFO(std::to_string(num_threads));
    CHECK(runner.Wait() < 25ns);
}

void RunRareUpdates(uint32_t num_threads) {
    std::atomic value = new int{};

    std::jthread updater{[&](std::stop_token token) {
        RegisterThread();

        auto i = 0;
        while (!token.stop_requested()) {
            Retire(value.exchange(new int{++i}));
            std::this_thread::sleep_for(1ms);
        }

        UnregisterThread();
        CHECK(i > 500);
    }};

    RunReadOnly(num_threads, &value);
}

}  // namespace

TEST_CASE("Read only") {
    for (auto num_threads : {1, 2, 4, 8, 16}) {
        std::atomic value = new int{1023};
        RunReadOnly(num_threads, &value);
    }
}

TEST_CASE("Rare updates") {
    for (auto num_threads : {1, 2, 4, 8, 16}) {
        RunRareUpdates(num_threads);
    }
}
