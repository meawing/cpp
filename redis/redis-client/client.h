#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <cactus/net/address.h>

namespace redis {

class Client {
public:
    explicit Client(const cactus::SocketAddress& server_address);

    std::optional<std::string> Get(std::string_view key);
    void Set(std::string_view key, std::string_view value);
    int64_t Increment(std::string_view key);
    int64_t Decrement(std::string_view key);
};

}  // namespace redis
