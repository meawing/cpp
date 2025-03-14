#pragma once

#include <stack>
#include <mutex>
#include <optional>

template <class T>
class MPSCQueue {
public:
    // Push adds one element to stack top.
    // Safe to call from multiple threads.
    void Push(T value) {
        std::lock_guard guard{mutex_};
        stack_.push(std::move(value));
    }

    // Pop removes top element from the stack.
    // Not safe to call concurrently.
    std::optional<T> Pop() {
        std::lock_guard guard{mutex_};
        if (stack_.empty()) {
            return std::nullopt;
        }
        std::optional result = std::move(stack_.top());
        stack_.pop();
        return result;
    }

    // DequeuedAll Pop's all elements from the stack and calls callback() for each.
    // Not safe to call concurrently with Pop()
    void DequeueAll(auto&& callback) {
        std::stack<T> stack;
        {
            std::lock_guard guard{mutex_};
            stack = std::move(stack_);
        }
        while (!stack.empty()) {
            callback(std::move(stack.top()));
            stack.pop();
        }
    }

private:
    std::stack<T> stack_;
    std::mutex mutex_;
};
