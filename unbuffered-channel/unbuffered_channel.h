#pragma once

#include <optional>

template <class T>
class UnbufferedChannel {
public:
    void Send(const T& value) {
    }

    void Send(T&& value) {
    }

    std::optional<T> Recv() {
        return std::nullopt;
    }

    void Close() {
    }
};
