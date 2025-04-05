#pragma once

#include <chrono>
#include <coroutine>
#include <functional>
#include <queue>

using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

struct Promise;

class Coroutine {
public:
    using Handle = std::coroutine_handle<Promise>;
    using promise_type = Promise;

    explicit Coroutine(Handle handle) : handle_{handle} {
    }

    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    Coroutine(Coroutine&& other) : handle_{other.handle_} {
        other.handle_ = nullptr;
    }
    Coroutine& operator=(Coroutine&& other) {
        if (this != &other) {
            Clear();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~Coroutine() {
        Clear();
    }

    void Resume() const {
        handle_.resume();
    }

    bool Done() const {
        return handle_.done();
    }

    void Clear() {
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }

private:
    Handle handle_;
};

struct Awaiter {
    bool await_ready() {
        return false;
    }
    void await_suspend(std::coroutine_handle<>) {
    }
    void await_resume() {
    }
};

struct Promise {
    Coroutine get_return_object() {
        return Coroutine{Coroutine::Handle::from_promise(*this)};
    }
    std::suspend_always initial_suspend() {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }
    void unhandled_exception() {
    }
    void return_void() {
    }
    Awaiter await_transform(TimePoint) {
        return {};
    }
    Awaiter await_transform(Duration duration) {
        return await_transform(std::chrono::system_clock::now() + duration);
    }
};

class Scheduler {
public:
    template <class... Args>
    void AddTask(auto&& task, Args&&... args) {
        coroutines_.push(task(std::forward<Args>(args)...));
    }

    bool Step() {
        if (coroutines_.empty()) {
            return false;
        }
        auto& c = coroutines_.front();
        c.Resume();
        if (c.Done()) {
            coroutines_.pop();
        }
        return true;
    }

    void Run() {
        while (Step()) {
        }
    }

private:
    std::queue<Coroutine> coroutines_;
};
