#include "unbuffered_channel.h"
#include "test_channel.h"

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
