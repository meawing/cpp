#include "unbuffered_channel.h"
#include "test_channel.h"
#include "util.h"

#include <thread>

using namespace std::chrono_literals;

namespace {

void RunTest(int senders_count, int receivers_count) {
    std::vector<std::jthread> threads;
    UnbufferedChannel<int> channel;
    std::atomic counter = 0;
    auto was_closed = false;
    std::mutex m;
    auto is_closed = [&] {
        std::lock_guard lock(m);
        return was_closed;
    };

    std::vector<std::vector<int>> send_values(senders_count);
    for (auto& values : send_values) {
        threads.emplace_back([&] {
            while (true) {
                auto value = counter.fetch_add(1, std::memory_order::relaxed);
                try {
                    channel.Send(value);
                    values.push_back(value);
                } catch (const std::runtime_error&) {
                    CHECK_MT(is_closed());
                    break;
                }
            }
        });
    }

    std::vector<std::vector<int>> recv_values(receivers_count);
    for (auto& values : recv_values) {
        threads.emplace_back([&] {
            while (auto value = channel.Recv()) {
                values.push_back(*value);
            }
            CHECK_MT(is_closed());
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    m.lock();
    channel.Close();
    was_closed = true;
    m.unlock();
    threads.clear();

    CheckValues(send_values, recv_values);
    if ((senders_count == 1) && (receivers_count == 1)) {
        REQUIRE(send_values[0] == recv_values[0]);
    }
}

enum class BlockType { kSender, kReceiver };

template <BlockType type>
void BlockRun() {
    static constexpr auto kNow = std::chrono::steady_clock::now;
    static constexpr auto kTimeLimit = 40ms;
    static constexpr auto kRange = std::views::iota(0, 30);

    UnbufferedChannel<int> channel;

    std::jthread sender{[&channel] {
        for (auto i : kRange) {
            if constexpr (type == BlockType::kReceiver) {
                std::this_thread::sleep_for(kTimeLimit);
            }
            auto start = kNow();
            channel.Send(i);
            if constexpr (type == BlockType::kSender) {
                auto elapsed = kNow() - start;
                CHECK_MT(std::chrono::abs(elapsed - kTimeLimit) < 10ms);
            }
        }
        channel.Close();
    }};

    std::jthread receiver{[&channel] {
        for (auto i : kRange) {
            if constexpr (type == BlockType::kSender) {
                std::this_thread::sleep_for(kTimeLimit);
            }
            auto start = kNow();
            auto value = channel.Recv();
            CHECK_MT((value && (*value == i)));
            if constexpr (type == BlockType::kReceiver) {
                auto elapsed = kNow() - start;
                CHECK_MT(std::chrono::abs(elapsed - kTimeLimit) < 10ms);
            }
        }
        auto value = channel.Recv();
        CHECK_MT(!value);
    }};
}

}  // namespace

TEST_CASE("Closing") {
    UnbufferedChannel<int> channel;
    TestClose(&channel);
}

TEST_CASE("Data race") {
    UnbufferedChannel<int> channel;
    TestDataRace(&channel);
}

TEST_CASE("Copy") {
    UnbufferedChannel<std::vector<int>> channel;
    TestCopy(&channel);
}

TEST_CASE("Move only") {
    UnbufferedChannel<std::unique_ptr<std::string>> channel;
    TestMoveOnly(&channel);
}

TEST_CASE("Simple") {
    RunTest(1, 1);
}

TEST_CASE("Senders") {
    RunTest(4, 1);
}

TEST_CASE("Receivers") {
    RunTest(1, 6);
}

TEST_CASE("BigBuf") {
    RunTest(3, 3);
}

TEST_CASE("BlockRunSender") {
    BlockRun<BlockType::kSender>();
}

TEST_CASE("BlockRunReceiver") {
    BlockRun<BlockType::kReceiver>();
}

TEST_CASE("Passive waiting") {
    UnbufferedChannel<int> channel;

    std::jthread sender([&] {
        CPUTimer timer{CPUTimer::THREAD};
        channel.Send(1);
        auto cpu_time = timer.GetTimes().TotalCpuTime();
        CHECK_MT(cpu_time < 1ms);
    });

    std::jthread receiver([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        channel.Recv();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{1000});

    channel.Close();
}

TEST_CASE("Rendezvous") {
    UnbufferedChannel<int> channel;
    std::atomic_flag finished{false};

    std::jthread sender([&] {
        channel.Send(42);
        finished.test_and_set();
    });

    std::this_thread::sleep_for(20ms);
    REQUIRE_FALSE(finished.test());
    REQUIRE(channel.Recv() == 42);
}

TEST_CASE("Spurious release") {
    UnbufferedChannel<int> channel;
    std::vector<std::jthread> senders;

    for (auto _ = 0; _ < 10'000; ++_) {
        std::mutex m;
        std::condition_variable recv_cond;
        std::condition_variable send_cond;
        bool received{false}, checked{false};
        int received_sender;

        for (auto released_sender = 0; released_sender < 2; ++released_sender) {
            senders.emplace_back([&, released_sender] {
                channel.Send(released_sender);
                std::unique_lock<std::mutex> _(m);
                send_cond.wait(_, [&] { return received; });
                REQUIRE(released_sender == received_sender);
                received = false;
                checked = true;
                recv_cond.notify_one();
            });
        }

        std::jthread receiver([&] {
            for (int i = 0; i < 2; ++i) {
                auto sender_id = channel.Recv();
                std::unique_lock<std::mutex> _(m);
                received_sender = *sender_id;
                received = true;
                send_cond.notify_one();
                recv_cond.wait(_, [&] { return checked; });
                checked = false;
            }
        });
        senders.clear();
    }
}
