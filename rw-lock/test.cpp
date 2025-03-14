#include "rw_lock.h"

#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <iostream>
#include <numeric>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

namespace {

constexpr auto kNow = std::chrono::steady_clock::now;

auto ElapsedTime(std::chrono::steady_clock::time_point start) {
    auto diff = kNow() - start;
    return std::chrono::duration_cast<std::chrono::duration<double>>(diff);
}

}  // namespace

TEST_CASE("Increment") {
    static constexpr auto kTimeLimit = 1s;
    static constexpr auto kNumThreadsInGroup = 8;
    RWLock rw_lock;
    std::vector<int> r_counters(kNumThreadsInGroup);
    std::vector<int> w_counters(kNumThreadsInGroup);

    auto total = 0;
    {
        std::atomic_ref total_a{total};
        std::vector<std::jthread> threads;
        for (auto i = 0; i < kNumThreadsInGroup; ++i) {
            threads.emplace_back([&, i] {
                auto start = kNow();
                while (ElapsedTime(start) < kTimeLimit) {
                    rw_lock.Read([&total_a, &c = r_counters[i]] {
                        ++c;
                        total_a.fetch_add(1);
                    });
                }
            });
            threads.emplace_back([&, i] {
                auto start = kNow();
                while (ElapsedTime(start) < kTimeLimit) {
                    rw_lock.Write([&total_a, &c = w_counters[i]] {
                        ++c;
                        total_a.fetch_add(1);
                    });
                }
            });
        }
        threads.clear();
    }

    REQUIRE(std::ranges::min(r_counters) > 1'000);
    REQUIRE(std::ranges::min(w_counters) > 200);
    auto read_count = std::reduce(r_counters.begin(), r_counters.end());
    auto write_count = std::reduce(w_counters.begin(), w_counters.end());
    REQUIRE(total == read_count + write_count);
    std::cout << "read count " << read_count << ", write count " << write_count << "\n";
}

TEST_CASE("RLock") {
    static constexpr auto kTimeLimit = .5s;
    static constexpr auto kReadFunc = [] { std::this_thread::sleep_for(kTimeLimit); };
    RWLock rw_lock;
    std::vector<std::jthread> threads;

    auto start = kNow();
    for (auto i = 0; i < 16; ++i) {
        threads.emplace_back([&rw_lock] { rw_lock.Read(kReadFunc); });
    }

    threads.clear();
    REQUIRE(ElapsedTime(start) < 2 * kTimeLimit);
}

TEST_CASE("OnlyWritingOrReading") {
    static constexpr auto kTimeLimit = 1s;
    RWLock rw_lock;
    std::atomic_flag is_writing;
    std::atomic num_reading = 0;
    std::atomic result = 0;
    std::vector<std::jthread> threads;

    for (auto i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            auto start = kNow();
            while (ElapsedTime(start) < kTimeLimit) {
                rw_lock.Write([&] {
                    result.fetch_or(is_writing.test());
                    is_writing.test_and_set();
                    result.fetch_or(num_reading.load());
                    is_writing.clear();
                });
            }
        });
        threads.emplace_back([&] {
            auto start = kNow();
            while (ElapsedTime(start) < kTimeLimit) {
                rw_lock.Read([&] {
                    num_reading.fetch_add(1);
                    result.fetch_or(is_writing.test());
                    num_reading.fetch_sub(1);
                });
            }
        });
    }

    threads.clear();
    REQUIRE(result.load() == 0);
}

TEST_CASE("Read-Die-Write") {
    RWLock rw_lock;

    auto flag = false;
    REQUIRE_THROWS_AS(rw_lock.Read([] { throw 42; }), int);

    rw_lock.Write([&] { flag = true; });
    CHECK(flag);
}

TEST_CASE("Read-Write-Die-Write") {
    constexpr auto kNumWriters = 100;

    RWLock rw_lock;
    auto counter = 0;
    std::atomic num_throws = 0;

    std::jthread reader{[&] {
        rw_lock.Read([&] {
            std::this_thread::sleep_for(100ms);
            CHECK(counter == 0);
        });
    }};

    std::this_thread::sleep_for(50ms);
    std::vector<std::jthread> writers;
    for (auto i = 0; i < kNumWriters; ++i) {
        writers.emplace_back([&] {
            try {
                rw_lock.Write([&] {
                    ++counter;
                    throw 4.2f;
                });
            } catch (float) {
                num_throws.fetch_add(1);
            } catch (...) {
            }
        });
    }

    writers.clear();
    CHECK(counter == kNumWriters);
    CHECK(num_throws.load() == kNumWriters);
}
