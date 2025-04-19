#include "../hazard-ptr/hazard_ptr.h"
#include "sync_map.h"
#include "runner.h"

#include <shared_mutex>
#include <unordered_map>
#include <ranges>
#include <chrono>
#include <atomic>
#include <string>

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace std::chrono_literals;

void RunSharedMutex(uint32_t num_threads) {
    std::unordered_map<int, int> map;
    for (auto i = 0; i < 1'024; ++i) {
        map[i] = i;
    }

    TimeRunner runner{1s, num_threads};
    std::shared_mutex mutex;
    auto is_error = false;

    for (auto i = 0u; i < num_threads; ++i) {
        auto func = [&, i, is_error = std::atomic_ref{is_error}] {
            if (std::shared_lock guard{mutex}; !map.contains(i)) {
                is_error.store(true, std::memory_order::relaxed);
            }
        };
        TaskWithExit task{std::move(func), UnregisterThread};
        runner.DoWithInit(RegisterThread, std::move(task));
    }

    INFO(std::to_string(num_threads));
    CHECK(runner.Wait() >= 30ns);
    CHECK_FALSE(is_error);
}

void RunSyncMap(uint32_t num_threads) {
    SyncMap<int, int> map;
    for (auto i = 0; i < 1'024; ++i) {
        map.Insert(i, i);
    }

    TimeRunner runner{1s, num_threads};
    auto is_error = false;

    for (auto i = 0u; i < num_threads; ++i) {
        auto func = [&, i, is_error = std::atomic_ref{is_error}, v = 0]() mutable {
            if (!map.Lookup(i, &v)) {
                is_error.store(true, std::memory_order::relaxed);
            }
        };
        TaskWithExit task{std::move(func), UnregisterThread};
        runner.DoWithInit(RegisterThread, std::move(task));
    }

    INFO(std::to_string(num_threads));
    CHECK(runner.Wait() < 15ns);
    CHECK_FALSE(is_error);
}

}  // namespace

TEST_CASE("Shared mutex") {
    for (auto num_threads : {4, 8, 16}) {
        RunSharedMutex(num_threads);
    }
}

TEST_CASE("Sync map") {
    for (auto num_threads : {4, 8, 16}) {
        RunSyncMap(num_threads);
    }
}
