#include <benchmark/benchmark.h>

#include <glog/logging.h>

#include <cactus/cactus.h>

using namespace cactus;

static const std::array<char, 4> kPing = {'p', 'i', 'n', 'g'};
static const std::array<char, 4> kPong = {'p', 'o', 'n', 'g'};

static const auto kLoopback = folly::SocketAddress(folly::IPAddressV4("127.0.0.1"), 0);
constexpr int kParallelRequests = 10;

void SimpleServer(IListener* lsn) {
    while (true) {
        auto client = lsn->Accept();

        DLOG(INFO) << "Accepted client " << client->RemoteAddress();

        std::array<char, 4> buf;
        client->ReadFull(View(buf));

        client->Write(View(kPong));

        DLOG(INFO) << "Finished handling client " << client->RemoteAddress();
    }
}

void ConcurrentServer(IListener* lsn) {
    ServerGroup g;
    while (true) {
        std::shared_ptr<IConn> client{lsn->Accept()};

        g.Spawn([client] {
            DLOG(INFO) << "Accepted client " << client->RemoteAddress();

            std::array<char, 4> buf;
            client->ReadFull(View(buf));

            client->Write(View(kPong));

            DLOG(INFO) << "Finished handling client " << client->RemoteAddress();
        });
    }
}

static void ServerBenchmark(benchmark::State& state) {
    Scheduler sched;
    sched.Run([&] {
        auto lsn = ListenTCP(kLoopback);
        Fiber server([&] {
            if (state.range(0) == 0) {
                SimpleServer(lsn.get());
            } else {
                ConcurrentServer(lsn.get());
            }
        });

        while (state.KeepRunningBatch(kParallelRequests)) {
            WaitGroup g;
            for (int i = 0; i < kParallelRequests; i++) {
                g.Spawn([&, i] {
                    auto conn = DialTCP(lsn->Address());

                    DLOG(INFO) << i << " sending ping";
                    conn->Write(View(kPing));

                    std::array<char, 4> buf;
                    conn->ReadFull(View(buf));
                    DLOG(INFO) << i << " received pong";
                });
            }

            g.Wait();
        }
    });
}

BENCHMARK(ServerBenchmark)->Arg(0)->Arg(1);

BENCHMARK_MAIN();
