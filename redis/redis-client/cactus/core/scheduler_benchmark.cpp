#include <benchmark/benchmark.h>

#include <thread>

#include "scheduler.h"

using namespace cactus;

static void SingleFiberYield(benchmark::State& state) {
    Scheduler scheduler;
    scheduler.Run([&] {
        for (auto _ : state) {
            Yield();
        }
    });
}

BENCHMARK(SingleFiberYield);

static void SleepingFibers(benchmark::State& state) {
    Scheduler scheduler;
    scheduler.Run([&] {
        std::vector<Fiber> fibers;
        for (int i = 0; i < state.range(0); ++i) {
            fibers.emplace_back([&] {
                while (true) {
                    TimeoutGuard timeout(std::chrono::seconds(1));
                    Yield();
                }
            });
        }

        while (state.KeepRunningBatch(state.range(0))) {
            Yield();
        }
    });
}

BENCHMARK(SleepingFibers)->Range(1, 1 << 17);

static void SleepingThreads(benchmark::State& state) {
    for (auto _ : state) {
        std::this_thread::yield();
    }
}

BENCHMARK(SleepingThreads)->ThreadRange(1, 1 << 13);

BENCHMARK_MAIN();
