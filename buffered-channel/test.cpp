#include "buffered_channel.h"
#include "test_channel.h"

static void RunTest(size_t senders_count, size_t receivers_count, int buff_size) {
    std::vector<std::jthread> threads;
    BufferedChannel<int> channel(buff_size);
    std::atomic counter = 0;
    std::atomic free_slots = buff_size;

    std::vector<std::vector<int>> send_values(senders_count);
    auto was_closed = false;
    std::mutex m;
    auto is_closed = [&] {
        std::lock_guard lock(m);
        return was_closed;
    };
    for (auto& values : send_values) {
        threads.emplace_back([&] {
            while (true) {
                auto value = counter.fetch_add(1, std::memory_order::relaxed);
                try {
                    channel.Send(value);
                    CHECK_MT(free_slots.fetch_sub(1, std::memory_order::relaxed) >= 0);
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
            while (true) {
                free_slots.fetch_add(1, std::memory_order::relaxed);
                if (auto value = channel.Recv()) {
                    values.push_back(*value);
                } else {
                    CHECK_MT(is_closed());
                    break;
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{300});
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

TEST_CASE("Closing") {
    BufferedChannel<int> channel{1};
    TestClose(&channel);
}

TEST_CASE("Data race") {
    BufferedChannel<int> channel{1};
    TestDataRace(&channel);
}

TEST_CASE("Copy") {
    BufferedChannel<std::vector<int>> channel{1};
    TestCopy(&channel);
}

TEST_CASE("Move only") {
    BufferedChannel<std::unique_ptr<std::string>> channel{1};
    TestMoveOnly(&channel);
}

TEST_CASE("Simple") {
    RunTest(1, 1, 1);
}

TEST_CASE("Senders") {
    RunTest(4, 1, 3);
}

TEST_CASE("Receivers") {
    RunTest(1, 6, 4);
}

TEST_CASE("SmallBuf") {
    RunTest(8, 2, 2);
}

TEST_CASE("BigBuf") {
    RunTest(3, 3, 100);
}

TEST_CASE("Random") {
    RunTest(8, 8, 8);
}
