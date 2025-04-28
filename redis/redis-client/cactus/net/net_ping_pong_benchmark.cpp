#include <benchmark/benchmark.h>

#include <glog/logging.h>

#include <cactus/cactus.h>

using namespace cactus;

static const auto kLoopback = folly::SocketAddress(folly::IPAddressV4("127.0.0.1"), 0);

static void ServerBenchmark(benchmark::State& state) {
    Scheduler sched;
    sched.Run([&] {
        auto lsn = ListenTCP(kLoopback);
        Fiber server([&] {
            auto conn = lsn->Accept();

            while (true) {
                int n[1] = {0};
                conn->ReadFull(View(n));
                n[0]++;
                conn->Write(View(n));
            }
        });

        auto conn = DialTCP(lsn->Address());
        int n = 0;

        while (state.KeepRunning()) {
            int i[1] = {n};

            conn->Write(View(i));

            conn->ReadFull(View(i));

            if (n + 1 != i[0]) {
                LOG(FATAL) << n << " + 2 != " << i[0];
            }

            n = i[0];
        }
    });
}

BENCHMARK(ServerBenchmark)->Arg(1);

BENCHMARK_MAIN();
