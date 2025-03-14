#include "rw_spinlock.h"

#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr auto kSleepTime = std::chrono::milliseconds{50};

using MFPtr = void (RWSpinLock::*)();

void Test(MFPtr main_lock, MFPtr main_unlock, MFPtr thread_lock) {
    auto flag = false;
    RWSpinLock lock;

    (lock.*main_lock)();
    std::thread t{[&] {
        (lock.*thread_lock)();
        flag = true;
    }};

    std::this_thread::sleep_for(kSleepTime);
    REQUIRE_FALSE(flag);
    (lock.*main_unlock)();
    t.join();
    REQUIRE(flag);
}

}  // namespace

TEST_CASE("ManyReads") {
    RWSpinLock lock;
    auto start = std::chrono::steady_clock::now();

    std::vector<std::jthread> threads;
    for (auto i = 0; i < 16; ++i) {
        threads.emplace_back([&] {
            lock.LockRead();
            std::this_thread::sleep_for(kSleepTime);
        });
    }
    threads.clear();

    auto diff = std::chrono::steady_clock::now() - start;
    REQUIRE(diff < 2 * kSleepTime);
}

TEST_CASE("WriteWrite") {
    Test(&RWSpinLock::LockWrite, &RWSpinLock::UnlockWrite, &RWSpinLock::LockWrite);
}

TEST_CASE("ReadWrite") {
    Test(&RWSpinLock::LockRead, &RWSpinLock::UnlockRead, &RWSpinLock::LockWrite);
}

TEST_CASE("WriteRead") {
    Test(&RWSpinLock::LockWrite, &RWSpinLock::UnlockWrite, &RWSpinLock::LockRead);
}

TEST_CASE("Stress") {
    static constexpr auto kNumThreads = 8u;
    static constexpr auto kNumIterations = 10'000u;

    RWSpinLock lock;
    auto state = 0u;
    auto read_f = [&] {
        auto sum = 0u;
        for (auto i = 0u; i < kNumIterations; ++i) {
            lock.LockRead();
            sum += state;
            lock.UnlockRead();
        }
        return sum;
    };

    auto write_f = [&] {
        for (auto i = 0u; i < kNumIterations; ++i) {
            lock.LockWrite();
            ++state;
            lock.UnlockWrite();
        }
    };

    std::vector<std::jthread> threads;
    for (auto i = 0u; i < kNumThreads; ++i) {
        threads.emplace_back(read_f);
        threads.emplace_back(write_f);
    }
    threads.clear();
    REQUIRE(state == kNumThreads * kNumIterations);
}
