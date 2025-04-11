// executor.h
#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <functional>
#include <exception>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>

// Used instead of void in generic code
struct Unit {};

class Task : public std::enable_shared_from_this<Task> {
public:
    Task() = default;
    virtual ~Task() {
    }

    // The user’s work
    virtual void Run() = 0;

    // Called before submission, adding dependencies that must be finished (either
    // Completed, Failed, or Canceled) before the task is eligible for execution
    void AddDependency(std::shared_ptr<Task> dep);
    // Called before submission, adding additional trigger conditions: the task
    // becomes eligible if at least one trigger finishes.
    void AddTrigger(std::shared_ptr<Task> dep);
    // Called before submission: sets a time point; when the deadline is reached,
    // the task is eligible to run.
    void SetTimeTrigger(std::chrono::system_clock::time_point at);

    // The following functions may be called from any thread:
    bool IsCompleted();
    bool IsFailed();
    bool IsCanceled();
    bool IsFinished();
    std::exception_ptr GetError();
    void Cancel();
    void Wait();

    // --- Methods used internally by the executor
    // Check the “ready condition” – note that a task is ready if any one of the following is true:
    //   * It has dependencies and all of these are finished (completed, failed or canceled)
    //   * It has one or more triggers and at least one trigger is finished 
    //   * A time trigger was set and the specified time point has passed.
    // In case no dependencies/triggers/time are set the task is immediately ready.
    bool IsReady();

    // Called by the Executor worker to run the task (if not canceled).
    void RunTask();

    // (Helpers to use inside Executor.)
    // Return the time trigger (if set) so Executor can decide a wait deadline.
    std::chrono::system_clock::time_point GetTimeTrigger();
    bool HasTimeTrigger();

    friend class Executor;

private:
    // States that a task may be in.
    enum class State {
        Pending,
        Running,
        Completed,
        Failed,
        Canceled
    };

    // Mark the task finished (used when canceled before executing)
    void MarkFinished();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    State state_ { State::Pending };
    std::exception_ptr error_ { nullptr };

    // dependencies and triggers are stored as weak pointers to avoid cycles.
    std::vector<std::weak_ptr<Task>> dependencies_;
    std::vector<std::weak_ptr<Task>> triggers_;

    // Time trigger (optional)
    bool has_time_trigger_ { false };
    std::chrono::system_clock::time_point time_trigger_;
};

// An executor is a thread pool running Tasks.
class Executor {
public:
    Executor(uint32_t num_threads);
    ~Executor();
    
    void Submit(std::shared_ptr<Task> task);

    void StartShutdown();
    void WaitShutdown();

private:
    void WorkerLoop();

    std::mutex mtx_;
    std::condition_variable cv_;
    bool shutdown_ { false };

    // pending tasks that are waiting for a dependency/trigger/time condition.
    std::deque<std::shared_ptr<Task>> pending_;
    // tasks that have become “ready” and need to run (or be canceled)
    std::deque<std::shared_ptr<Task>> ready_;
    std::vector<std::thread> workers_;
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads);
