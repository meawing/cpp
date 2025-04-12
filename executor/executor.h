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
#include <stdexcept>
#include <numeric>
#include <algorithm>
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
    virtual void Cancel();
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
    std::atomic<State> state_{ State::Pending };
    // error_ is still protected by mutex_, so we leave it as-is.
    std::exception_ptr error_ { nullptr };

    // dependencies and triggers are stored as weak pointers to avoid cycles.
    std::vector<std::weak_ptr<Task>> dependencies_;
    std::vector<std::weak_ptr<Task>> triggers_;

    // Time trigger (optional)
    bool has_time_trigger_ { false };
    std::chrono::system_clock::time_point time_trigger_;
};

template <class T>
class Future;  // forward declaration

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
public:
    ThenFuture(FuturePtr<T> input, std::function<Y()> fn)
        : Future<Y>(fn), input_(input), user_fn_(std::move(fn))
    {
        // Make this task wait for the input to finish.
        this->AddDependency(input_);
    }

    virtual void Run() override {
        // At this point, the dependency is finished.
        // Propagate error if input failed.
        if (input_->IsFailed()) {
            std::rethrow_exception(input_->GetError());
        }
        if (input_->IsCanceled()) { 
            this->Cancel();
            return;
        }
        // Optionally, one could call input_->Get() here to retrieve the input value.
        // Then call user_fn_ (which in our design does not take arguments).
        this->result_ = user_fn_();
    }
private:
    FuturePtr<T> input_;
    std::function<Y()> user_fn_;
};

// --------------------------
// WhenAllFuture: waits for all input futures to finish and collects their results.
template <class T>
class WhenAllFuture : public Future<std::vector<T>> {
public:
    WhenAllFuture(std::vector<FuturePtr<T>> futures)
        : Future<std::vector<T>>([&](){ return std::vector<T>{}; }), futures_(std::move(futures))
    {
        // Make this task depend on each future.
        for (auto &f : futures_) {
            this->AddDependency(f);
        }
    }
    
    // NEW OVERRIDE: When canceled, also cancel all children.
    void Cancel() override {
        // Call base cancellation.
        Future<std::vector<T>>::Cancel();
        // Propagate cancellation to each sub-future.
        for (auto &f : futures_) {
            f->Cancel();
        }
    }
    
    virtual void Run() override {
        std::vector<T> results;
        // Collect results from the fired futures.
        for (auto &f : futures_) {
            // We ignore futures that are canceled or failed.
            if (!f->IsCanceled() && !f->IsFailed()) {
                results.push_back(f->Get());
            }
        }
        this->result_ = results;
    }
private:
    std::vector<FuturePtr<T>> futures_;
};

// --------------------------
// WhenFirstFuture: waits until any one of its input futures is finished (using triggers)
// and returns its result.
template <class T>
class WhenFirstFuture : public Future<T> {
public:
    WhenFirstFuture(std::vector<FuturePtr<T>> futures)
        : Future<T>([&](){ return T{}; }), futures_(std::move(futures))
    {
        // Instead of dependencies (which require all to be finished),
        // add each future as a trigger so that this task becomes ready when any of them finish.
        for (auto &f : futures_) {
            this->AddTrigger(f);
        }
    }

    virtual void Run() override {
        // Busy-wait checking which future has finished.
        // In practice one might want to avoid spun loops.
        while (true) {
            for (auto &f : futures_) {
                if (f->IsFinished()) {
                    if (f->IsFailed()) {
                        std::rethrow_exception(f->GetError());
                    }
                    if (f->IsCanceled()) {
                        this->Cancel();
                        return;
                    }
                    this->result_ = f->Get();
                    return;
                }
            }
            std::this_thread::yield();
        }
    }
private:
    std::vector<FuturePtr<T>> futures_;
};

// --------------------------
// WhenAllBeforeDeadlineFuture: collects results of futures that have finished before a given deadline.
template <class T>
class WhenAllBeforeDeadlineFuture : public Future<std::vector<T>> {
public:
    WhenAllBeforeDeadlineFuture(std::vector<FuturePtr<T>> futures,
                                  std::chrono::system_clock::time_point deadline)
        : Future<std::vector<T>>([&](){ return std::vector<T>{}; }),
          futures_(std::move(futures)),
          deadline_(deadline)
    {
        // The task should become ready either when all of its input futures finish
        // or when the deadline is reached.
        this->SetTimeTrigger(deadline_);
        for (auto& f : futures_) {
            this->AddDependency(f);
        }
    }

    virtual void Run() override {
        std::vector<T> results;
        for (auto &f : futures_) {
            if (f->IsFinished()) {
                // Only add valid (non canceled/failed) results.
                if (!f->IsCanceled() && !f->IsFailed()) {
                    results.push_back(f->Get());
                }
            }
        }
        this->result_ = results;
    }
    
private:
    std::vector<FuturePtr<T>> futures_;
    std::chrono::system_clock::time_point deadline_;
};

// ***************************
// Executor definition
// ***************************
class Executor {
public:
    Executor(uint32_t num_threads);
    ~Executor();
    
    void Submit(std::shared_ptr<Task> task);

    void StartShutdown();
    void WaitShutdown();

    // --- Future combinators:

    // Invoke: execute the provided function and return a Future for the result.
    template <class T>
    FuturePtr<T> Invoke(std::function<T()> fn) {
        auto future = std::make_shared<Future<T>>(std::move(fn));
        Submit(future);
        return future;
    }

    // Then: execute fn after input has completed.
    template <class Y, class T>
    FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn) {
        auto future = std::make_shared<ThenFuture<Y, T>>(input, std::move(fn));
        Submit(future);
        return future;
    }

    // WhenAll: given several futures, return a future with the vector of results.
    template <class T>
    FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all) {
        auto future = std::make_shared<WhenAllFuture<T>>(std::move(all));
        Submit(future);
        return future;
    }

    // WhenFirst: returns a future with the result of whichever of the supplied futures finishes first.
    template <class T>
    FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all) {
        auto future = std::make_shared<WhenFirstFuture<T>>(std::move(all));
        Submit(future);
        return future;
    }

    // WhenAllBeforeDeadline: returns a future with results that are ready before the deadline.
    template <class T>
    FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
                                                    std::chrono::system_clock::time_point deadline) {
        auto future = std::make_shared<WhenAllBeforeDeadlineFuture<T>>(std::move(all), deadline);
        Submit(future);
        return future;
    }

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


