#include "unbuffered_channel.h"
#include "run_channel.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Benchmark") {
    constexpr auto kNumThreads = 16;
    for (auto readers_count : {2, 8, 14}) {
        auto senders_count = kNumThreads - readers_count;
        int64_t sum{};
        BENCHMARK(std::to_string(readers_count)) {
            UnbufferedChannel<int> channel;
            sum = CalcSum(&channel, senders_count, readers_count);
        };
        CHECK(sum == kSum);
    }
}
