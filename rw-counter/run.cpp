#include "rw_counter.h"
#include "runner.h"

#include <vector>
#include <future>
#include <ranges>
#include <stop_token>
#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Benchmark") {
    static constexpr auto kNumWriterThreads = 15;
    ReadWriteAtomicCounter counter;
    std::stop_source stop;

    std::vector<std::future<size_t>> fututes;
    for (auto i = 0; i < kNumWriterThreads; ++i) {
        fututes.push_back(std::async(std::launch::async, [&] {
            auto result = 0ul;
            auto incrementer = counter.GetIncrementer();
            while (!stop.stop_requested()) {
                incrementer->Increment();
                ++result;
            }
            return result;
        }));
    }
    fututes.push_back(std::async(std::launch::async, [&] {
        auto result = 0ul;
        [[maybe_unused]] auto sum = 0ll;
        while (!stop.stop_requested()) {
            sum += counter.GetValue();
            ++result;
        }
        return result;
    }));

    std::this_thread::sleep_for(std::chrono::seconds{1});
    stop.request_stop();

    auto num_reads = fututes.back().get();
    auto num_writes = 0ul;
    for (auto& f : fututes | std::views::take(kNumWriterThreads)) {
        num_writes += f.get();
    }

    CHECK(num_reads > 500'000);
    CHECK(num_writes > 100'000'000);
}
