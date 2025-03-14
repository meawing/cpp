#include "rw_counter.h"

#include <vector>
#include <thread>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Simple") {
    ReadWriteAtomicCounter counter;
    REQUIRE(counter.GetValue() == 0);

    auto i1 = counter.GetIncrementer();
    i1->Increment();
    REQUIRE(counter.GetValue() == 1);

    auto i2 = counter.GetIncrementer();
    i2->Increment();
    REQUIRE(counter.GetValue() == 2);

    i1->Increment();
    REQUIRE(counter.GetValue() == 3);

    counter.GetIncrementer()->Increment();
    REQUIRE(counter.GetValue() == 4);
}

TEST_CASE("Data race") {
    ReadWriteAtomicCounter counter;
    std::jthread t1{[&] { counter.GetIncrementer(); }};
    std::jthread t2{[&] { counter.GetValue(); }};
}

TEST_CASE("Increments") {
    static constexpr auto kNumIncrements = 100'000;
    static constexpr auto kNumThreads = 10;

    ReadWriteAtomicCounter counter;

    std::vector<std::jthread> threads;
    for (auto i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&] {
            auto incrementer = counter.GetIncrementer();
            for (auto i = 0; i < kNumIncrements; ++i) {
                incrementer->Increment();
            }
        });
    }
    threads.clear();

    REQUIRE(counter.GetValue() == kNumIncrements * kNumThreads);
}
