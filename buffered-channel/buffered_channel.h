#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <condition_variable>
#include <stdexcept>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(uint32_t size)
        : open_(true), size_(size), waiting_push_(0), waiting_pop_(0) {
    }
    void Send(const T& value) {
        std::unique_lock lock(mutex_);
        if (!open_) {
            throw std::runtime_error("error");
        }
        WaitSend(lock);
        queue_.push(value);
        if (waiting_pop_ > 0) {
            cv_pop_.notify_all();
        }
    }

    void Send(T&& value) {
        std::unique_lock lock(mutex_);
        if (!open_) {
            throw std::runtime_error("error");
        }
        WaitSend(lock);
        queue_.push(std::move(value));
        if (waiting_pop_ > 0) {
            cv_pop_.notify_all();
        }
    }

    std::optional<T> Recv() {
        std::unique_lock lock(mutex_);
        ++waiting_pop_;
        while (queue_.empty() && (open_ || waiting_push_ > 0)) {
            cv_pop_.wait(lock);
        }
        --waiting_pop_;

        if (!queue_.empty()) {
            auto res = std::move(queue_.front());
            queue_.pop();

            if (waiting_push_ > 0) {
                cv_push_.notify_one();
            }

            return res;
        }

        return std::nullopt;
    }

    void Close() {
        std::lock_guard lg(mutex_);
        open_ = false;
        if (waiting_pop_ > 0) {
            cv_pop_.notify_all();
        }
    }

private:
    void WaitSend(std::unique_lock<std::mutex>& lock) {
        ++waiting_push_;
        while (queue_.size() == size_) {
            cv_push_.wait(lock);
        }
        --waiting_push_;
    }

    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
    std::mutex mutex_;
    bool open_;
    size_t size_;
    std::queue<T> queue_;
    size_t waiting_push_;
    size_t waiting_pop_;
};
