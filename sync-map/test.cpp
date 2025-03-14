#include "../hazard-ptr/hazard_ptr.h"
#include "sync_map.h"

#include <random>
#include <unordered_map>
#include <thread>
#include <latch>
#include <vector>
#include <chrono>
#include <atomic>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Simple") {
    RegisterThread();
    SyncMap<int, int> map;

    int value;
    REQUIRE_FALSE(map.Lookup(0, &value));

    REQUIRE(map.Insert(0, 42));
    REQUIRE(map.Lookup(0, &value));
    REQUIRE(value == 42);

    REQUIRE_FALSE(map.Insert(0, 42));
    UnregisterThread();
}

TEST_CASE("UpgradeToReadOnly") {
    RegisterThread();
    SyncMap<int, int> map;

    map.Insert(0, 42);
    map.Insert(1, 42);
    map.Insert(2, 42);

    for (auto i = 0; i < 1024; ++i) {
        int value{};
        REQUIRE(map.Lookup(0, &value));
        REQUIRE(value == 42);
    }
    UnregisterThread();
}

TEST_CASE("Stress") {
    std::unordered_map<int, int> std_map;
    SyncMap<int, int> map;

    std::mt19937_64 gen{42};
    std::uniform_int_distribution dist{0, 100'000};

    for (auto i = 0; i < 1'000'000; ++i) {
        auto key = dist(gen);
        int value;

        auto it = std_map.find(key);
        auto found = map.Lookup(key, &value);
        if (it == std_map.end()) {
            REQUIRE_FALSE(found);
        } else {
            REQUIRE(found);
            REQUIRE(value == it->second);
        }

        if (!found && (dist(gen) % 2)) {
            value = dist(gen);
            map.Insert(key, value);
            std_map[key] = value;
        }
    }
}

namespace {

struct StressMultiThread {
    void Run() {
        std::vector<std::jthread> threads;

        for (auto i = 0u; i < kNumReaders; ++i) {
            threads.emplace_back([&, i] { RunReader(i); });
        }
        for (auto i = 0u; i < kNumWriters; ++i) {
            threads.emplace_back([&, i] { RunWriter(i); });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        latch_.count_down();
    }

    std::atomic_bool is_fail;
    std::vector<std::unordered_map<int, int>> looked{kNumReaders};
    std::vector<std::unordered_map<int, int>> inserted{kNumWriters};

private:
    void RunReader(size_t index) {
        RegisterThread();

        auto& local_map = looked[index];
        std::mt19937_64 gen{index};
        std::uniform_int_distribution dist{1, kMaxValue};
        int value;
        latch_.wait();

        for (auto i = 0; i < 100'000; ++i) {
            auto key = dist(gen);
            auto it = local_map.find(key);
            if (map_.Lookup(key, &value)) {
                if ((it != local_map.end()) && (it->second != value)) {
                    is_fail.store(true, std::memory_order::relaxed);
                    break;
                }
                local_map[key] = value;
            } else if (it != local_map.end()) {
                is_fail.store(true, std::memory_order::relaxed);
                break;
            }
        }

        UnregisterThread();
    }

    void RunWriter(size_t index) {
        auto& local_map = inserted[index];
        RegisterThread();

        std::mt19937_64 gen{kNumReaders + index};
        std::uniform_int_distribution dist{1, kMaxValue};
        latch_.wait();

        for (auto i = 0; i < 100'000; ++i) {
            auto key = dist(gen);
            if (map_.Insert(key, index)) {
                if (local_map.contains(key)) {
                    is_fail.store(true, std::memory_order::relaxed);
                    break;
                }
                local_map[key] = index;
            }
        }

        UnregisterThread();
    }

    static constexpr auto kNumReaders = 100u;
    static constexpr auto kNumWriters = 3u;
    static constexpr auto kMaxValue = 10'000;

    SyncMap<int, int> map_;
    std::latch latch_{1};
};

}  // namespace

TEST_CASE_METHOD(StressMultiThread, "StressMultiThread") {
    Run();
    REQUIRE_FALSE(is_fail.load());

    std::unordered_map<int, int> values;
    for (const auto& m : inserted) {
        for (const auto& [k, v] : m) {
            REQUIRE(values.emplace(k, v).second);
        }
    }

    for (const auto& m : looked) {
        for (const auto& [k, v] : m) {
            auto it = values.find(k);
            REQUIRE(it != values.end());
            REQUIRE(v == it->second);
        }
    }
}
