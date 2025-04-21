#pragma once

#include <vector>
#include <memory>

#include <cactus/cactus.h>

class IBalancer {
public:
    virtual ~IBalancer() = default;
    virtual void SetBackends(const std::vector<cactus::SocketAddress>& peers) = 0;
    virtual void Run() = 0;
    virtual const cactus::SocketAddress& GetAddress() const = 0;
};

std::unique_ptr<IBalancer> CreateBalancer(const cactus::SocketAddress& address);
