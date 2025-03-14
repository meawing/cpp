#include "buffered_channel.h"
#include "run_channel.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Benchmark") {
    for (auto buff_size : {2, 10, 100'000}) {
        int64_t sum{};
        BENCHMARK(std::to_string(buff_size)) {
            BufferedChannel<int> channel(buff_size);
            sum = CalcSum(&channel, 8, 8);
        };
        CHECK(sum == kSum);
    }
}
