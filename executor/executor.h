// executor.h
#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <atomic>
#include <exception>
#include <functional>

struct Unit {};

class Executor;

class Task : public std::enable_shared_from_this<Task> {
public:
    Task() = default;
    virtual ~Task() = default;

    virtual void Run() = 0;

    void AddDependency(std::shared_ptr<Task> dep);
    void AddTrigger(std::shared_ptr<Task> trigger);
    void SetTimeTrigger(std::chrono::system_clock::time_point at);

    bool IsCompleted() const { return state_.load() == State::Completed; }
    bool IsFailed() const { return state_.load() == State::Failed; }
    bool IsCanceled() const { return state_.load() == State::Canceled; }
    bool IsFinished() const { 
        auto s = state_.load();
        return s == State::Completed || s == State::Failed || s == State::Canceled;
    }

    std::exception_ptr GetError() const {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(mutex_));
        return error_;
    }

    void Cancel();
    void Wait();

    friend class Executor;

protected:
    enum class State {
        Pending,
        Running,
        Completed,
        Failed,
        Canceled
    };

    void RunTask();
    void NotifyDependencyFinished();
    void NotifyTriggerFinished();
    void MarkFinished();

    std::atomic<State> state_{State::Pending};
    mutable std::mutex mutex_;  // Made mutable
    std::condition_variable cv_;
    std::exception_ptr error_{nullptr};

    std::atomic<unsigned> remaining_deps_{0};
    std::atomic<bool> trigger_fired_{false};
    std::atomic<bool> has_trigger_{false};
    std::atomic<bool> has_time_trigger_{false};
    std::chrono::system_clock::time_point time_trigger_;

    mutable std::mutex dependents_mutex_;  // Made mutable
    std::vector<std::weak_ptr<Task>> dependents_;
    std::vector<std::weak_ptr<Task>> trigger_dependents_;

    std::weak_ptr<Executor> executor_;
};


class Executor : public std::enable_shared_from_this<Executor> {
public:
    Executor(uint32_t num_threads);
    ~Executor();

    void Submit(std::shared_ptr<Task> task);
    void StartShutdown();
    void WaitShutdown();
    void Enqueue(std::shared_ptr<Task> task);                                                  

private:
    void WorkerLoop();

    mutable std::mutex queue_mutex_;  // Made mutable
    std::condition_variable queue_cv_;
    std::deque<std::shared_ptr<Task>> ready_queue_;
    std::atomic<bool> shutdown_{false};
    std::vector<std::thread> workers_;
    
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads);
