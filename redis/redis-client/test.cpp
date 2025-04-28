#include "client.h"

#include <utility>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <tuple>
#include <optional>

#include <cactus/cactus.h>
#include <cactus/test.h>

#include <catch2/generators/catch_generators.hpp>

namespace {

using Script = std::vector<std::pair<std::string_view, std::string_view>>;

class MockServer {
public:
    MockServer(Script script, bool no_throw)
        : lsn_{cactus::ListenTCP({"127.0.0.1", 0})},
          fiber_{MakeFiber(std::move(script), no_throw)} {
    }

    const cactus::SocketAddress& GetAddress() const {
        return lsn_->Address();
    }

private:
    cactus::Fiber MakeFiber(Script&& script, bool no_throw) {
        return cactus::Fiber{[this, script = std::move(script), no_throw] {
            if (no_throw) {
                CHECK_NOTHROW(Run(script));
            } else {
                Run(script);
            }
        }};
    }

    void Run(const Script& script) {
        auto conn = lsn_->Accept();
        std::string buffer;
        for (const auto& [expected_req, rsp] : script) {
            buffer.resize(expected_req.size());
            {
                cactus::TimeoutGuard guard{std::chrono::milliseconds{100}};
                conn->ReadFull(cactus::View(buffer));
            }
            REQUIRE(buffer == expected_req);
            for (auto c : rsp) {
                conn->Write(cactus::View(c));
                cactus::Yield();
            }
        }
    }

    std::unique_ptr<cactus::IListener> lsn_;
    cactus::Fiber fiber_;
};

auto MakeTest(Script script, bool no_throw = true) {
    MockServer server{std::move(script), no_throw};
    const auto& address = server.GetAddress();
    return std::pair<redis::Client, MockServer>{std::piecewise_construct,
                                                std::forward_as_tuple(address),
                                                std::forward_as_tuple(std::move(server))};
}

}  // namespace

FIBER_TEST_CASE("Get") {
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$3\r\nGET\r\n"
                                  "$3\r\nkey\r\n",
                                  "$5\r\nvalue\r\n"},
                                 {"*2\r\n"
                                  "$3\r\nGET\r\n"
                                  "$7\r\nnot-key\r\n",
                                  "$-1\r\n"},
                                 {"*2\r\n"
                                  "$3\r\nGET\r\n"
                                  "$4\r\nsome\r\n",
                                  "-ERR some error\r\n"}});
    REQUIRE(client.Get("key") == "value");
    REQUIRE(client.Get("not-key") == std::nullopt);
    REQUIRE_THROWS_AS(client.Get("some"), redis::RedisError);
}

FIBER_TEST_CASE("GetBadResponse") {
    auto bad_response = GENERATE("aba", ":3\r\nfoo\r\n", "$3\r\nfoo\n\r", "$2\r\nfoo\r\n");
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$3\r\nGET\r\n"
                                  "$4\r\nkey2\r\n",
                                  bad_response}},
                                false);
    REQUIRE_THROWS_AS(client.Get("key2"), redis::RedisError);
}

FIBER_TEST_CASE("Set") {
    auto [client, _] = MakeTest({{"*3\r\n"
                                  "$3\r\nSET\r\n"
                                  "$3\r\nkey\r\n"
                                  "$5\r\nvalue\r\n",
                                  "$2\r\nOK\r\n"},
                                 {"*3\r\n"
                                  "$3\r\nSET\r\n"
                                  "$4\r\nkey1\r\n"
                                  "$6\r\nvalue1\r\n",
                                  "-ERR Error\r\n"}});
    REQUIRE_NOTHROW(client.Set("key", "value"));
    REQUIRE_THROWS_AS(client.Set("key1", "value1"), redis::RedisError);
}

FIBER_TEST_CASE("SetBadResponse") {
    auto bad_response = GENERATE("bar", "$6\r\nNOT_OK\r\n", ":2\r\nOK\r\n", "$2\r\nOK\n\r");
    auto [client, _] = MakeTest({{"*3\r\n"
                                  "$3\r\nSET\r\n"
                                  "$7\r\nfoo_bar\r\n"
                                  "$4\r\nabcd\r\n",
                                  bad_response}},
                                false);
    REQUIRE_THROWS_AS(client.Set("foo_bar", "abcd"), redis::RedisError);
}

FIBER_TEST_CASE("Increment") {
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$4\r\nINCR\r\n"
                                  "$3\r\nkey\r\n",
                                  ":1\r\n"},
                                 {"*2\r\n"
                                  "$4\r\nINCR\r\n"
                                  "$3\r\nkey\r\n",
                                  ":2\r\n"},
                                 {"*2\r\n"
                                  "$4\r\nINCR\r\n"
                                  "$4\r\nkey1\r\n",
                                  "-ERR Incr error\r\n"}});
    REQUIRE(client.Increment("key") == 1);
    REQUIRE(client.Increment("key") == 2);
    REQUIRE_THROWS_AS(client.Increment("key1"), redis::RedisError);
}

FIBER_TEST_CASE("IncrBadResponse") {
    auto bad_response = GENERATE("caba", "$2\r\nOK\r\n", ":4aba\r\n", ":-8\n\r");
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$4\r\nINCR\r\n"
                                  "$5\r\n a b \r\n",
                                  bad_response}},
                                false);
    REQUIRE_THROWS_AS(client.Increment(" a b "), redis::RedisError);
}

FIBER_TEST_CASE("Decrement") {
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$4\r\nDECR\r\n"
                                  "$3\r\nkey\r\n",
                                  ":-1\r\n"},
                                 {"*2\r\n"
                                  "$4\r\nDECR\r\n"
                                  "$3\r\nkey\r\n",
                                  ":-2\r\n"},
                                 {"*2\r\n"
                                  "$4\r\nDECR\r\n"
                                  "$4\r\nkey1\r\n",
                                  "-ERR Another error\r\n"}});
    REQUIRE(client.Decrement("key") == -1);
    REQUIRE(client.Decrement("key") == -2);
    REQUIRE_THROWS_AS(client.Decrement("key1"), redis::RedisError);
}

FIBER_TEST_CASE("DecrBadResponse") {
    auto bad_response = GENERATE("caba", "$2\r\nOK\r\n", ":4aba\r\n", ":-8\n\r");
    auto [client, _] = MakeTest({{"*2\r\n"
                                  "$4\r\nDECR\r\n"
                                  "$4\r\nc  d\r\n",
                                  bad_response}},
                                false);
    REQUIRE_THROWS_AS(client.Decrement("c  d"), redis::RedisError);
}
