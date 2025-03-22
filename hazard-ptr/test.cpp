#include "hazard_ptr.h"

#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

#include <catch2/catch_test_macros.hpp>

namespace {

struct State {
    explicit State(int value) : value{value} {
        num.fetch_add(1, std::memory_order::relaxed);
    }
    ~State() {
        num.fetch_sub(1, std::memory_order::relaxed);
    }
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    static inline std::atomic num = 0;
    int value;
};

void Run(std::atomic<State*>* value) {
    static std::mutex mutex;
    static std::mutex mutex_check;
    static auto x = 0;

    RegisterThread();

    auto last_read = 0;
    for (auto i = 0; i < 100'000; ++i) {
        if (i % 123 == 0) {
            State* old_value;
            {
                std::lock_guard guard{mutex};
                old_value = value->exchange(new State{++x});
            }
            Retire(old_value);
        } else {
            if (auto* p = Acquire(value)) {
                std::lock_guard guard{mutex_check};
                CHECK(p->value >= last_read);
                last_read = p->value;
            }
            Release();
        }
    }
    Retire(value->exchange(nullptr));

    UnregisterThread();
}

}  // namespace

TEST_CASE("SingleThread") {
    RegisterThread();

    std::atomic value = new int{42};
    auto* p = Acquire(&value);
    REQUIRE(*p == 42);

    for (auto i = 0; i < 100; ++i) {
        Retire(value.exchange(new int{100500}));
    }
    Retire(value.exchange(nullptr));

    REQUIRE(*p == 42);
    Release();

    UnregisterThread();
}

TEST_CASE("ManyThreads") {
    for (auto i = 0; i < 5; ++i) {
        std::atomic<State*> value = nullptr;

        std::vector<std::jthread> threads;
        for (auto j = 0; j < 10; ++j) {
            threads.emplace_back(Run, &value);
        }
        threads.clear();

        REQUIRE(State::num.load() == 0);
    }
}
