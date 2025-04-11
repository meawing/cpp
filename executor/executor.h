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

// -----------------------------------------------------------------------------
// Base Task and Future
// -----------------------------------------------------------------------------
struct Unit {};

template <class T>
class Future;  // forward declaration

class Executor; // forward declaration

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
    void AddDependency(std::shared_ptr<Task> dep);
    void AddTrigger(std::shared_ptr<Task> trigger);
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

    // Called by dependencies/triggers when they finish.
    void NotifyDependencyFinished();
    void NotifyTriggerFinished();

    // --- Called by the Executor when the task is submitted.
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

    // Dependency and trigger members:
    std::atomic<unsigned> remainingDeps_{0};
    std::atomic<bool> triggerFired_{false};
    bool has_trigger_ { false }; 

    std::mutex dependents_mutex_;
    std::vector<std::weak_ptr<Task>> dependents_;        // tasks waiting on this as dependency
    std::vector<std::weak_ptr<Task>> trigger_dependents_;  // tasks waiting on this as trigger

    // Optional time trigger.
    bool has_time_trigger_ { false };
    std::chrono::system_clock::time_point time_trigger_;

    // Pointer to executor used to enqueue the task as soon as ready.
    Executor* executor_ { nullptr };
};

// Base Future implementation.
// Future is a Task that computes a result.
template <class T>
class Future : public Task {
public:
    // Constructor wrapping a function.
    Future(std::function<T()> fn) 
        : func_(std::move(fn)) 
    {}

    // Gets the result, blocking until the computation finishes.
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
    // Default Run() calls the wrapped function.
    virtual void Run() override {
        result_ = func_();
    }
    std::function<T()> func_;
    T result_;
};

// -----------------------------------------------------------------------------
// Combinator Futures
// -----------------------------------------------------------------------------

// ThenFuture: Executes a functor after its dependency has finished.
template <class Y, class T>
class ThenFuture : public Future<Y> {
public:
    // Constructor takes the functor.
    ThenFuture(std::function<Y()> fn)
        : Future<Y>(std::move(fn))
    {}

    virtual void Run() override {
        // At this point the dependency is finished.
        // Simply execute the functor.
        this->result_ = this->func_();
    }
};

// WhenAllFuture: Waits for all input futures to finish and collects their results.
template <class T>
class WhenAllFuture : public Future<std::vector<T>> {
public:
    WhenAllFuture(const std::vector<typename Future<T>::Ptr>& fs)
        : futures_(fs)
    {}

    virtual void Run() override {
        std::vector<T> results;
        results.reserve(futures_.size());
        for (auto &f : futures_) {
            // f->Get() will throw if an exception occurred in any future.
            results.push_back(f->Get());
        }
        this->result_ = std::move(results);
    }
    using Ptr = std::shared_ptr<WhenAllFuture<T>>;
private:
    std::vector<typename Future<T>::Ptr> futures_;
};

// WhenFirstFuture: Waits until any one of the input futures is finished.
template <class T>
class WhenFirstFuture : public Future<T> {
public:
    WhenFirstFuture(const std::vector<typename Future<T>::Ptr>& fs)
        : futures_(fs)
    {}

    virtual void Run() override {
        // At least one trigger must have fired.
        // Find the first future that is finished and get its result.
        for (auto &f : futures_) {
            if (f->IsFinished()) {
                this->result_ = f->Get();
                break;
            }
        }
    }
    using Ptr = std::shared_ptr<WhenFirstFuture<T>>;
private:
    std::vector<typename Future<T>::Ptr> futures_;
};

// WhenAllBeforeDeadlineFuture: At a given time deadline, collects results
// from input futures that have finished before the deadline.
template <class T>
class WhenAllBeforeDeadlineFuture : public Future<std::vector<T>> {
public:
    WhenAllBeforeDeadlineFuture(const std::vector<typename Future<T>::Ptr>& fs,
                                  std::chrono::system_clock::time_point deadline)
        : futures_(fs), deadline_(deadline)
    {
        // Set a time trigger so that the task becomes eligible at deadline.
        this->SetTimeTrigger(deadline_);
    }

    virtual void Run() override {
        std::vector<T> results;
        for (auto &f : futures_) {
            if (f->IsFinished()) {
                // Collect result if the future finished.
                results.push_back(f->Get());
            }
        }
        this->result_ = std::move(results);
    }
    using Ptr = std::shared_ptr<WhenAllBeforeDeadlineFuture<T>>;
private:
    std::vector<typename Future<T>::Ptr> futures_;
    std::chrono::system_clock::time_point deadline_;
};

// Helper alias for FuturePtr.
template <class T>
using FuturePtr = std::shared_ptr<Future<T>>;

// To simplify usage in combinators, add a public alias Ptr to Future.
template <class T>
class FutureWithPtr : public Future<T> {
public:
    using Ptr = std::shared_ptr<Future<T>>;
};

// -----------------------------------------------------------------------------
// Executor
// -----------------------------------------------------------------------------

class Executor {
public:
    Executor(uint32_t num_threads);
    ~Executor();

    // Submit a task. The task may have dependencies or triggers.
    void Submit(std::shared_ptr<Task> task);

    // Signal shutdown. Tasks not yet started become canceled.
    void StartShutdown();
    // Block until all worker threads exit.
    void WaitShutdown();

    // Called from Task callbacks (dependency/trigger finished)
    void Enqueue(std::shared_ptr<Task> task);

    // -------- Future combinators ----------

    // Invoke: execute the provided function and return a Future for the result.
    template <class T>
    FuturePtr<T> Invoke(std::function<T()> fn) {
        auto fut = std::make_shared<Future<T>>(std::move(fn));
        Submit(fut);
        return fut;
    }

    // Then: execute fn after input has completed.
    template <class Y, class T>
    FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn) {
        auto fut = std::make_shared<ThenFuture<Y, T>>(std::move(fn));
        // Register the dependency. With the revised AddDependency,
        // if input is already finished the dependency is immediately considered satisfied.
        fut->AddDependency(input);
        Submit(fut);
        return fut;
    }

    // WhenAll: given several futures, return a future with the vector of results.
    template <class T>
    FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all) {
        auto fut = std::make_shared<WhenAllFuture<T>>(all);
        // Add dependency on each input future.
        for (auto &f : all) {
            fut->AddDependency(f);
        }
        Submit(fut);
        return fut;
    }

    // WhenFirst: returns a future with the result of whichever future finishes first.
    template <class T>
    FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all) {
        auto fut = std::make_shared<WhenFirstFuture<T>>(all);
        // Register a trigger on each input future.
        for (auto &f : all) {
            fut->AddTrigger(f);
        }
        Submit(fut);
        return fut;
    }

    // WhenAllBeforeDeadline: returns a future with results that are ready before the deadline.
    template <class T>
    FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
                                                    std::chrono::system_clock::time_point deadline) {
        auto fut = std::make_shared<WhenAllBeforeDeadlineFuture<T>>(all, deadline);
        // No dependency registrations are used here so that the time trigger brings it in.
        Submit(fut);
        return fut;
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
