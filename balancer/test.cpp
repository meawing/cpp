#include "balancer.h"
#include "util.h"

#include <ranges>
#include <vector>
#include <memory>
#include <utility>
#include <random>

#include <cactus/test.h>

using namespace std::chrono_literals;

namespace {

class SquaringPeers {
public:
    explicit SquaringPeers(size_t count) : counts_(count), lsns_(count), addresses_(count) {
        for (auto i : std::views::iota(size_t{}, count)) {
            lsns_[i] = cactus::ListenTCP({"127.0.0.1", 0});
            addresses_[i] = lsns_[i]->Address();
            group_.Spawn([&, lsn = lsns_[i].get(), i] { RunPeer(lsn, i); });
        }
    }

    void Kill(size_t index) {
        lsns_.at(index)->Close();
    }

    const std::vector<size_t>& GetCounts() const {
        return counts_;
    }

    const std::vector<cactus::SocketAddress>& GetAddresses() const {
        return addresses_;
    }

private:
    void RunPeer(cactus::IListener* lsn, size_t index) {
        while (auto conn = TryGetConnection(lsn)) {
            ++counts_[index];
            group_.Spawn([this, conn = std::move(conn), index] {
                try {
                    HandleClient(conn.get());
                } catch (...) {
                }
                --counts_[index];
            });
        }
    }

    static std::shared_ptr<cactus::IConn> TryGetConnection(cactus::IListener* lsn) {
        try {
            return lsn->Accept();
        } catch (...) {
            return nullptr;
        }
    }

    static void HandleClient(cactus::IConn* conn) {
        while (true) {
            uint32_t num;
            conn->ReadFull(cactus::View(num));
            conn->Write(cactus::View(num * num));
        }
    }

    std::vector<size_t> counts_;
    std::vector<std::shared_ptr<cactus::IListener>> lsns_;
    std::vector<cactus::SocketAddress> addresses_;
    cactus::ServerGroup group_;
};

auto CreateServers(size_t num_peers) {
    auto peers = std::make_unique<SquaringPeers>(num_peers);
    auto balancer = CreateBalancer({"127.0.0.1", 0});
    balancer->SetBackends(peers->GetAddresses());
    balancer->Run();
    cactus::SleepFor(100ms);
    return std::make_pair(std::move(balancer), std::move(peers));
}

void TestConnection(cactus::IConn* conn, uint32_t req) {
    conn->Write(cactus::View(req));
    uint32_t rsp;
    conn->ReadFull(cactus::View(rsp));
    CHECK(rsp == req * req);
}

class RequestsExecutor {
public:
    explicit RequestsExecutor(const cactus::SocketAddress& address) : address_{address} {
    }

    void AddClient() {
        auto i = static_cast<uint32_t>(conns_.size());
        auto conn = conns_.emplace_back(cactus::DialTCP(address_)).get();
        TestConnection(conn, i);
    }

    void AddClients(size_t count) {
        size_t num_success = 0;
        cactus::WaitGroup group;
        for (size_t i = 0; i < count; ++i) {
            group.Spawn([&] {
                AddClient();
                ++num_success;
            });
        }
        group.Wait();
        CHECK(num_success == count);
    }

    void TestAllConnections() {
        for (auto i = 0u; auto& conn : conns_) {
            TestConnection(conn.get(), ++i);
        }
    }

private:
    cactus::SocketAddress address_;
    std::vector<std::unique_ptr<cactus::IConn>> conns_;
};

}  // namespace

FIBER_TEST_CASE("OnePeer") {
    auto [balancer, peers] = CreateServers(1);
    auto& count = peers->GetCounts()[0];
    RequestsExecutor req{balancer->GetAddress()};

    for (auto i : std::views::iota(1, 101)) {
        req.AddClient();
        CHECK(count == static_cast<size_t>(i));
    }
}

FIBER_TEST_CASE("TwoPeers") {
    auto [balancer, peers] = CreateServers(2);
    const auto& count_first = peers->GetCounts()[0];
    const auto& count_second = peers->GetCounts()[1];

    RequestsExecutor req{balancer->GetAddress()};

    for (auto i : std::views::iota(1, 101)) {
        req.AddClient();
        CHECK(count_first == static_cast<size_t>(i));

        req.AddClient();
        CHECK(count_second == static_cast<size_t>(i));
    }

    req.TestAllConnections();
}

FIBER_TEST_CASE("ManyPeers") {
    auto [balancer, peers] = CreateServers(100);
    RequestsExecutor req{balancer->GetAddress()};

    for (auto i : std::views::iota(1, 11)) {
        for (const auto& c : peers->GetCounts()) {
            req.AddClient();
            REQUIRE(c == static_cast<size_t>(i));
        }
    }

    req.TestAllConnections();
}

FIBER_TEST_CASE("ManyClientsSimultaneously") {
    constexpr auto kNumPeers = 10;
    constexpr auto kNumClientsPerPeer = 20;
    constexpr auto kNumClients = kNumPeers * kNumClientsPerPeer;

    auto [balancer, peers] = CreateServers(kNumPeers);
    RequestsExecutor req{balancer->GetAddress()};
    req.AddClients(kNumClients);

    for (auto c : peers->GetCounts()) {
        CHECK(c == kNumClientsPerPeer);
    }
}

FIBER_TEST_CASE("DeadPeer") {
    auto [balancer, peers] = CreateServers(3);
    const auto& counts = peers->GetCounts();
    const auto& count_first = counts[0];
    const auto& count_second = counts[1];
    const auto& count_third = counts[2];

    RequestsExecutor req{balancer->GetAddress()};

    auto check = [&](size_t first, size_t second, size_t third) {
        CHECK(count_first == first);
        CHECK(count_second == second);
        CHECK(count_third == third);
        req.TestAllConnections();
    };

    req.AddClients(30);
    check(10, 10, 10);

    SECTION("First") {
        peers->Kill(0);
        req.AddClients(30);
        check(10, 25, 25);
    }

    SECTION("Second") {
        peers->Kill(1);
        req.AddClients(30);
        check(25, 10, 25);
    }

    SECTION("Third") {
        peers->Kill(2);
        req.AddClients(30);
        check(25, 25, 10);
    }

    SECTION("FirstAndThird") {
        peers->Kill(0);
        peers->Kill(2);
        req.AddClients(30);
        check(10, 40, 10);
    }

    SECTION("SecondThanFirst") {
        peers->Kill(1);
        req.AddClients(30);
        check(25, 10, 25);

        peers->Kill(0);
        req.AddClients(30);
        check(25, 10, 55);
    }

    SECTION("ByOne") {
        req.AddClient();
        peers->Kill(1);
        check(11, 10, 10);
        req.AddClient();
        check(11, 10, 11);
    }

    SECTION("All") {
        peers->Kill(0);
        peers->Kill(1);
        peers->Kill(2);
        CHECK_THROWS_AS(req.AddClient(), std::system_error);
    }
}

FIBER_TEST_CASE("ValidConnectionAfterDead") {
    auto [balancer, peers] = CreateServers(2);
    const auto& address = balancer->GetAddress();

    auto conn1 = cactus::DialTCP(address);
    auto conn2 = cactus::DialTCP(address);
    TestConnection(conn1.get(), 10);
    TestConnection(conn2.get(), 20);
    CHECK(peers->GetCounts() == std::vector<size_t>{1, 1});

    peers->Kill(0);
    cactus::SleepFor(20ms);
    TestConnection(conn1.get(), 100);

    auto conn3 = cactus::DialTCP(address);
    TestConnection(conn3.get(), 300);
    CHECK(peers->GetCounts() == std::vector<size_t>{1, 2});

    peers->Kill(1);
    cactus::SleepFor(20ms);
    TestConnection(conn1.get(), 100);
    TestConnection(conn2.get(), 200);
}

FIBER_TEST_CASE("CheckTimeout") {
    constexpr auto kConnectionTimeout = 100ms;
    constexpr auto kNumPeers = 10;

    std::vector<cactus::SocketAddress> addresses;
    for (auto i : std::views::iota(1, kNumPeers)) {
        addresses.emplace_back("240.0.0.1", i);
    }

    SquaringPeers peers{1};
    addresses.push_back(peers.GetAddresses()[0]);

    auto balancer = CreateBalancer({"127.0.0.1", 0});
    balancer->SetBackends(addresses);
    balancer->Run();
    cactus::SleepFor(100ms);

    {
        CPUTimer timer;

        auto conn = cactus::DialTCP(balancer->GetAddress());
        TestConnection(conn.get(), 67);

        auto times = timer.GetTimes();
        auto wall_time = times.wall_time;

        CHECK(wall_time < kNumPeers * kConnectionTimeout);
        CHECK(wall_time > (kNumPeers - 1) * kConnectionTimeout);
        CHECK(times.TotalCpuTime() < wall_time / 10);
    }

    {
        CPUTimer timer;
        auto conn = cactus::DialTCP(balancer->GetAddress());

        TestConnection(conn.get(), 70);
        CHECK(timer.GetTimes().wall_time < kConnectionTimeout);
    }
}

FIBER_TEST_CASE("Stress") {
    constexpr auto kNumPeers = 10;

    auto [balancer, peers] = CreateServers(kNumPeers);
    const auto& counts = peers->GetCounts();
    auto expected = counts;

    std::vector<std::pair<std::unique_ptr<cactus::IConn>, size_t>> clients;
    std::mt19937_64 gen{15};
    std::uniform_int_distribution dist{1, 10};

    for (auto i : std::views::iota(0, 1'000)) {
        if (100 * dist(gen) > i) {
            auto index = std::ranges::min_element(counts) - counts.begin();

            auto conn = cactus::DialTCP(balancer->GetAddress());
            clients.emplace_back(std::move(conn), index);

            ++expected[index];
            while (counts[index] != expected[index]) {
                cactus::Yield();
            }
        } else if (!clients.empty()) {
            auto client_index = std::uniform_int_distribution{size_t{}, clients.size() - 1}(gen);
            std::swap(clients[client_index], clients.back());

            auto [conn, index] = std::move(clients.back());
            clients.pop_back();
            conn->Close();

            --expected[index];
            while (counts[index] != expected[index]) {
                cactus::Yield();
            }
        }
        REQUIRE(counts == expected);
    }
}

FIBER_TEST_CASE("Reconfiguration") {
    SquaringPeers initial_peers{3};
    SquaringPeers secondary_peers{4};
    SquaringPeers third_peer{1};
    const auto& initial_counts = initial_peers.GetCounts();
    const auto& secondary_counts = secondary_peers.GetCounts();

    auto server = CreateBalancer({"127.0.0.1", 0});
    server->SetBackends(initial_peers.GetAddresses());
    server->Run();

    cactus::SleepFor(100ms);
    RequestsExecutor req{server->GetAddress()};

    req.AddClients(50);
    REQUIRE(initial_counts == std::vector<size_t>{17, 17, 16});

    server->SetBackends(secondary_peers.GetAddresses());
    req.AddClients(50);
    REQUIRE(secondary_counts == std::vector<size_t>{13, 13, 12, 12});

    secondary_peers.Kill(0);
    secondary_peers.Kill(2);
    req.AddClients(50);
    REQUIRE(secondary_counts == std::vector<size_t>{13, 38, 12, 37});

    server->SetBackends(third_peer.GetAddresses());
    req.AddClients(50);
    REQUIRE(initial_counts == std::vector<size_t>{17, 17, 16});
    REQUIRE(secondary_counts == std::vector<size_t>{13, 38, 12, 37});
    REQUIRE(third_peer.GetCounts() == std::vector<size_t>{50});

    third_peer.Kill(0);
    auto conn = cactus::DialTCP(server->GetAddress());
    REQUIRE_THROWS_AS(TestConnection(conn.get(), 0), std::system_error);

    req.TestAllConnections();
}
