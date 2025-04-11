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

template <class T>
class Future;  // forward declaration

class Executor; // forward declaration

// ----------------------------------------------------------------------------
// FastTask: redesigned Task that is dependency-aware
// ----------------------------------------------------------------------------
class Task : public std::enable_shared_from_this<Task> {
public:
    Task() = default;
    virtual ~Task() {
        // Virtual destructor needed so that dependencies registered
        // on this task do not hold dangling pointers.
    }

    // User’s work
    virtual void Run() = 0;

    // --- Interface for adding dependencies and triggers (before submission)
    // When adding a dependency, the dependent task will later be notified
    // automatically when the 'dep' finishes.
    void AddDependency(std::shared_ptr<Task> dep);
    // Adding a trigger: the task will become ready when any trigger finishes.
    void AddTrigger(std::shared_ptr<Task> trigger);
    // Setting a time trigger: at the given time the task becomes eligible.
    void SetTimeTrigger(std::chrono::system_clock::time_point at);

    // --- Status queries (thread-safe)
    bool IsCompleted();
    bool IsFailed();
    bool IsCanceled();
    bool IsFinished();
    std::exception_ptr GetError();

    // Cancel the task. If it has not yet started, never call Run.
    virtual void Cancel();

    // Wait until the task is finished (Completed, Failed, or Canceled).
    void Wait();

    // Called only by Executor after the task is ready to run.
    void RunTask();

    // Intended for Executor to get the time trigger.
    std::chrono::system_clock::time_point GetTimeTrigger();
    bool HasTimeTrigger();

    // --- Called by a dependency/trigger that finished.
    // When a dependency finishes successfully (or fails or cancels),
    // the dependent task is notified.
    void NotifyDependencyFinished();
    // For triggers, when at least one trigger finishes.
    void NotifyTriggerFinished();

    // ----------------------------------------------------------------
    // (Internal members needed for fast dependency resolution)
    // ----------------------------------------------------------------
    // Called by the Executor when the task is submitted. This only stores
    // the executor pointer so that later notifications can enque the task.
    void SetExecutor(Executor* exec) { executor_ = exec; }

    friend class Executor;

protected:
    // These methods are used exclusively under the protection of mutex_.
    enum class State {
        Pending,
        Running,
        Completed,
        Failed,
        Canceled
    };

    // Mark the task finished (used for cancellations that happen before run)
    void MarkFinished();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    State state_{State::Pending};
    std::exception_ptr error_{nullptr};

    // The following are used for the dependency/trigger mechanism.
    // Instead of storing all dependencies and re‐scanning at every scheduling cycle,
    // we store counters and lists of “child” tasks that depend on this one.
    std::atomic<unsigned> remainingDeps_{0};
    std::atomic<bool> triggerFired_{false};

   
    // New flag: indicates that one or more triggers were added.
    bool has_trigger_ { false }; 

    // For tasks that depend on this task (via dependency or trigger), we register callbacks.
    // (Using weak_ptr to avoid cycles.)
    std::mutex dependents_mutex_;
    std::vector<std::weak_ptr<Task>> dependents_;        // tasks waiting on this as dependency
    std::vector<std::weak_ptr<Task>> trigger_dependents_;  // tasks waiting on this as trigger

    // Optionally, a single time trigger.
    bool has_time_trigger_ { false };
    std::chrono::system_clock::time_point time_trigger_;

    // Pointer to executor used to enqueue the task as soon as ready.
    Executor* executor_ { nullptr };
};

template <class T>
using FuturePtr = std::shared_ptr<Future<T>>;

// ***************************
// Future Implementation
// ***************************
//
// Future is a Task that computes a result value. Its Run() method (invoked by the Executor)
// calls an internal functor. It stores the result in a protected member so that helper classes
// (e.g. ThenFuture) can assign to it.
template <class T>
class Future : public Task {
public:
    // Constructor for a basic future wrapping a function.
    Future(std::function<T()> fn) 
        : func_(std::move(fn)) 
    {}

    // Gets the result, waiting for the computation to finish.
    T Get() {
        Wait();
        if (IsFailed()) {
            std::rethrow_exception(GetError());
        }
        if (IsCanceled()) {
            throw std::runtime_error("Future was canceled");
        }
        return result_;
    }
protected:
    // By default, Run() simply calls the wrapped function.
    virtual void Run() override {
        result_ = func_();
    }
    std::function<T()> func_;
    // Make the result available to derived classes.
    T result_;
};

// --------------------------
// ThenFuture: used by Executor::Then.
// It executes the provided functor after its dependency (input) has finished.
// It adds the dependency in its constructor so that it is not scheduled until input is done.
template <class Y, class T>
class ThenFuture : public Future<Y> {
};

// --------------------------
// WhenAllFuture: waits for all input futures to finish and collects their results.
template <class T>
class WhenAllFuture : public Future<std::vector<T>> {
};

// --------------------------
// WhenFirstFuture: waits until any one of its input futures is finished (using triggers)
// and returns its result.
template <class T>
class WhenFirstFuture : public Future<T> {
};

// --------------------------
// WhenAllBeforeDeadlineFuture: collects results of futures that have finished before a given deadline.
template <class T>
class WhenAllBeforeDeadlineFuture : public Future<std::vector<T>> {
};

// ----------------------------------------------------------------------------
// Fast Executor: uses a ready queue and events to schedule tasks
// ----------------------------------------------------------------------------
class Executor {
public:
    Executor(uint32_t num_threads);
    ~Executor();

    // Submit a task. The task may depend on other tasks or a time trigger.
    // If it is already ready (i.e. has no unsatisfied dependencies, or a trigger
    // is already finished or the time trigger has expired), it is immediately enqueued.
    void Submit(std::shared_ptr<Task> task);

    // Signal shutdown. Any tasks not yet started become canceled.
    void StartShutdown();
    // Block until all worker threads exit.
    void WaitShutdown();

    // This method is called from Task callbacks (dependency/trigger finished)
    // to schedule tasks that have become ready.
    void Enqueue(std::shared_ptr<Task> task);

    // --- Future combinators:

    // Invoke: execute the provided function and return a Future for the result.
    template <class T>
    FuturePtr<T> Invoke(std::function<T()> fn) {
    }

    // Then: execute fn after input has completed.
    template <class Y, class T>
    FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn) {
    }

    // WhenAll: given several futures, return a future with the vector of results.
    template <class T>
    FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all) {
    }

    // WhenFirst: returns a future with the result of whichever of the supplied futures finishes first.
    template <class T>
    FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all) {
    }


    // WhenAllBeforeDeadline: returns a future with results that are ready before the deadline.
    template <class T>
    FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
                                                    std::chrono::system_clock::time_point deadline) {
                                                    }
  

private:
    void WorkerLoop();

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::shared_ptr<Task>> ready_queue_;

    std::mutex exec_mutex_;
    bool shutdown_ { false };

    std::vector<std::thread> workers_;
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads);
