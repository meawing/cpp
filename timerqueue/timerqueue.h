#pragma once

#include <chrono>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

template <class T>
class TimerQueue {
    struct Item {
        bool operator<(const Item& other) const {
            return at_ > other.at_;
        }
        std::chrono::system_clock::time_point at_;
        mutable T value_;
    };

public:
    template <class... Args>
    void Add(std::chrono::system_clock::time_point at, Args&&... args) {
        std::unique_lock lock(mut_);
        queue_.push({at, T{std::forward<Args>(args)...}});
        // queue_.emplace(at, std::forward<Args>(args)...);
        cv_.notify_all();
    }

    T Pop() {
        std::unique_lock lock(mut_);
        while (queue_.empty() || queue_.top().at_ > std::chrono::system_clock::now()) {
            if (queue_.empty()) {
                cv_.wait(lock, [this] { return !queue_.empty(); });
            } else {
                auto time = queue_.top().at_;
                cv_.wait_until(lock, time);
            }
        }
        auto res = std::move(queue_.top().value_);
        queue_.pop();
        return res;
    }

private:
    std::priority_queue<Item> queue_;
    std::mutex mut_;
    std::condition_variable cv_;
};
