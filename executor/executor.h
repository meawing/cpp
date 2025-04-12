#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <exception>
#include <deque>
#include <mutex>
#include <condition_variable>

class Executor;

struct TaskState {
    enum Value {
        Pending,
        Running,
        Completed,
        Failed,
        Canceled
    };
};

class Task : public std::enable_shared_from_this<Task> {
public:
    Task() = default;
    virtual ~Task() = default;

    virtual void Run() = 0;

    void AddDependency(std::shared_ptr<Task> dep);
    void AddTrigger(std::shared_ptr<Task> trigger);
    void SetTimeTrigger(std::chrono::system_clock::time_point at);

    bool IsCompleted() const { return state_.load() == TaskState::Completed; }
    bool IsFailed() const { return state_.load() == TaskState::Failed; }
    bool IsCanceled() const { return state_.load() == TaskState::Canceled; }
    bool IsFinished() const { 
        auto s = state_.load();
        return s != TaskState::Pending && s != TaskState::Running;
    }

    std::exception_ptr GetError() const;
    void Cancel();
    void Wait();

    friend class Executor;

private:
    void RunTask();
    void NotifyDependencyFinished();
    void NotifyTriggerFinished();
    void MarkFinished();

    std::atomic<TaskState::Value> state_{TaskState::Pending};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::exception_ptr error_{nullptr};

    std::atomic<unsigned> remaining_deps_{0};
    std::atomic<bool> trigger_fired_{false};
    std::atomic<bool> has_trigger_{false};
    std::atomic<bool> has_time_trigger_{false};
    std::chrono::system_clock::time_point time_trigger_;

    // Single consolidated dependents list with type flag
    struct Dependent {
        std::weak_ptr<Task> task;
        bool is_trigger;
    };
    mutable std::mutex dependents_mutex_;
    std::vector<Dependent> dependents_;

    std::weak_ptr<Executor> executor_;
};

template <class T>
class Future;

template <class T>
using FuturePtr = std::shared_ptr<Future<T>>;

// Used instead of void in generic code
struct Unit {};


class Executor : public std::enable_shared_from_this<Executor> {
public:
    Executor(uint32_t num_threads);
    ~Executor();

    void Submit(std::shared_ptr<Task> task);
    void StartShutdown();
    void WaitShutdown();

    void Enqueue(std::shared_ptr<Task> task, uint32_t preferred_thread = 0);

    template <class T>
    FuturePtr<T> Invoke(std::function<T()> fn);

    template <class Y, class T>
    FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn);

    template <class T>
    FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all);

    template <class T>
    FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all);

    template <class T>
    FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
                                                    std::chrono::system_clock::time_point deadline);

    
private:
    struct ThreadLocalQueue {
        std::deque<std::shared_ptr<Task>> tasks;
        std::mutex mutex;
    };

    void WorkerLoop(uint32_t thread_index);
    bool TryStealWork(std::shared_ptr<Task>& task, uint32_t thief_index);
    

    std::vector<ThreadLocalQueue> thread_queues_;
    std::atomic<bool> shutdown_{false};
    std::vector<std::thread> workers_;
    std::atomic<uint32_t> next_thread_{0};
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads);

template <class T>
class Future : public Task {
public:
    T Get();

private:
};
