#pragma once

#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    explicit Semaphore(int count) : count_{count} {
    }

    void Acquire(auto callback) {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return count_; });
        callback(count_);
    }

    void Acquire() {
        Acquire([](int& value) { --value; });
    }

    void Release() {
        std::lock_guard lock{mutex_};
        ++count_;
        cv_.notify_one();
    }

private:
    int count_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
