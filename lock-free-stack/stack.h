#pragma once

#include <stack>
#include <mutex>

template <class T>
class Stack {
public:
    void Push(const T& value) {
        std::lock_guard guard{mutex_};
        data_.push(value);
    }

    bool Pop(T* value) {
        std::lock_guard guard{mutex_};
        if (data_.empty()) {
            return false;
        }
        *value = std::move(data_.top());
        data_.pop();
        return true;
    }

private:
    std::stack<T> data_;
    mutable std::mutex mutex_;
};
