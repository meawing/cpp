#include "executor.h"
#include <cassert>
#include <stdexcept>
#include <chrono>

// ----------------------------------------------------------------------------
// Implementation of Task methods
// ----------------------------------------------------------------------------

void Task::AddDependency(std::shared_ptr<Task> dep) {
    // Increase dependency count
    remainingDeps_.fetch_add(1, std::memory_order_relaxed);
    // Register self as a dependent on the dependency.
    {
        std::lock_guard<std::mutex> lk(dep->dependents_mutex_);
        dep->dependents_.push_back(shared_from_this());
    }
    // If the dependency is already finished, immediately notify that it has finished.
    if (dep->IsFinished()) {
        NotifyDependencyFinished();
    }
}

void Task::AddTrigger(std::shared_ptr<Task> trigger) {
    {
        std::lock_guard<std::mutex> lk(trigger->dependents_mutex_);
        trigger->trigger_dependents_.push_back(shared_from_this());
    }
    {
        std::lock_guard<std::mutex> lk(mutex_);
        has_trigger_ = true;
    }
}

void Task::SetTimeTrigger(std::chrono::system_clock::time_point at) {
    std::lock_guard<std::mutex> lk(mutex_);
    has_time_trigger_ = true;
    time_trigger_ = at;
}

// Status queries
bool Task::IsCompleted() {
    std::lock_guard<std::mutex> lk(mutex_);
    return state_ == State::Completed;
}
bool Task::IsFailed() {
    std::lock_guard<std::mutex> lk(mutex_);
    return state_ == State::Failed;
}
bool Task::IsCanceled() {
    std::lock_guard<std::mutex> lk(mutex_);
    return state_ == State::Canceled;
}
bool Task::IsFinished() {
    std::lock_guard<std::mutex> lk(mutex_);
    return (state_ == State::Completed ||
            state_ == State::Failed ||
            state_ == State::Canceled);
}
std::exception_ptr Task::GetError() {
    std::lock_guard<std::mutex> lk(mutex_);
    return error_;
}

void Task::Cancel() {
    bool shouldNotifyDependents = false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        // Only act if still pending.
        if (state_ != State::Pending)
            return;
        state_ = State::Canceled;
        shouldNotifyDependents = true;
    }
    // Notify waiters.
    cv_.notify_all();

    if (shouldNotifyDependents) {
        // Collect dependents waiting on this task.
        std::vector<std::shared_ptr<Task>> toNotify;
        {
            std::lock_guard<std::mutex> lk(dependents_mutex_);
            for (auto& weak_t : dependents_) {
                if (auto t = weak_t.lock())
                    toNotify.push_back(t);
            }
            dependents_.clear();

            // Also notify tasks waiting on this as a trigger.
            for (auto& weak_t : trigger_dependents_) {
                if (auto t = weak_t.lock())
                    toNotify.push_back(t);
            }
            trigger_dependents_.clear();
        }
        // Notify each dependent that a dependency has finished.
        for (auto& dependent : toNotify) {
            dependent->NotifyDependencyFinished();
            // For triggers, you might also want to notify if needed.
            dependent->NotifyTriggerFinished();
        }
    }
}

void Task::Wait() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [this] {
        return state_ != State::Pending && state_ != State::Running;
    });
}

// Called by Executor to run the task.
void Task::RunTask() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (state_ != State::Pending)
            return; // already canceled
        state_ = State::Running;
    }
    try {
        Run();
        {
            std::lock_guard<std::mutex> lk(mutex_);
            state_ = State::Completed;
        }
    } catch (...) {
        std::lock_guard<std::mutex> lk(mutex_);
        error_ = std::current_exception();
        state_ = State::Failed;
    }
    cv_.notify_all();

    // Notify tasks that depend on this one (dependency and trigger callbacks)
    std::vector<std::shared_ptr<Task>> toNotify;
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        for (auto& weak_t : dependents_) {
            if (auto t = weak_t.lock())
                toNotify.push_back(t);
        }
        dependents_.clear();
    }
    for (auto& t : toNotify) {
        t->NotifyDependencyFinished();
    }

    // For triggers, we notify all tasks waiting on this trigger.
    toNotify.clear();
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        for (auto& weak_t : trigger_dependents_) {
            if (auto t = weak_t.lock())
                toNotify.push_back(t);
        }
        trigger_dependents_.clear();
    }
    for (auto& t : toNotify) {
        t->NotifyTriggerFinished();
    }
}

std::chrono::system_clock::time_point Task::GetTimeTrigger() {
    std::lock_guard<std::mutex> lk(mutex_);
    return time_trigger_;
}

bool Task::HasTimeTrigger() {
    std::lock_guard<std::mutex> lk(mutex_);
    return has_time_trigger_;
}

// Called when one dependency has finished. If that was the last dependency
// OR a trigger has already fired, schedule the task.
void Task::NotifyDependencyFinished() {
    if (remainingDeps_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this());
        }
    }
}

// Called when one trigger has finished. When the first trigger fires, the task
// becomes ready even if some dependencies are still pending.
void Task::NotifyTriggerFinished() {
    bool expected = false;
    if (triggerFired_.compare_exchange_strong(expected, true)) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this());
        }
    }
}

// Mark as finished if not already running.
void Task::MarkFinished() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (state_ == State::Pending)
        state_ = State::Canceled;
    cv_.notify_all();
}

// ----------------------------------------------------------------------------
// Implementation of Executor
// ----------------------------------------------------------------------------

Executor::Executor(uint32_t num_threads) {
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { this->WorkerLoop(); });
    }
}

Executor::~Executor() {
    StartShutdown();
    WaitShutdown();
}

void Executor::Submit(std::shared_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        if (shutdown_) {
            task->Cancel();
            return;
        }
        task->SetExecutor(shared_from_this());  // Changed to shared_from_this
    }
    // task->SetExecutor(this);
    bool ready = false;
    auto now = std::chrono::system_clock::now();
    if (task->remainingDeps_.load(std::memory_order_relaxed) == 0 &&
        (!task->HasTimeTrigger() || (now >= task->GetTimeTrigger()))) {
        // Only treat the task as ready if it has no trigger attached
        // or if a trigger has already fired.
        {
            std::lock_guard<std::mutex> lk(task->mutex_);
            if (!task->has_trigger_ || task->triggerFired_.load(std::memory_order_relaxed)) {
                ready = true;
            }
        }
    }
    if (ready) {
        Enqueue(task);
    } else if (task->HasTimeTrigger()) {
        // If the only blocking condition is the time trigger, schedule a callback.
        auto deadline = task->GetTimeTrigger();
        auto delay = deadline - now;
        // Run a detached thread to sleep until the task's time trigger expires.
        std::thread([task, delay, this]() {
            std::this_thread::sleep_for(delay);
            // Once the deadline is reached, re-check that no dependencies hold it back.
            if (task->remainingDeps_.load(std::memory_order_relaxed) == 0) {
                // Now the time trigger is expired, so enqueue the task.
                this->Enqueue(task);
            }
        }).detach();
    }
    // Otherwise, the task will be enqueued later when notified by its dependencies or triggers.
}

void Executor::StartShutdown() {
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        shutdown_ = true;
    }
    queue_cv_.notify_all();
}

void Executor::WaitShutdown() {
    for (auto& t : workers_) {
        if (t.joinable())
            t.join();
    }
}

// Called by tasks when they become ready.
void Executor::Enqueue(std::shared_ptr<Task> task) {
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (shutdown_) {
        task->Cancel();
        return;
    }
    ready_queue_.push_back(std::move(task));
    queue_cv_.notify_one();
}

void Executor::WorkerLoop() {
    while (true) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this] {
                return shutdown_ || !ready_queue_.empty();
            });
            if (shutdown_ && ready_queue_.empty())
                break;
            task = ready_queue_.front();
            ready_queue_.pop_front();
        }
        if (task) {
            if (task->IsCanceled()) {
                task->MarkFinished();
            } else {
                task->RunTask();
            }
        }
    }
}

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads) {
    return std::make_shared<Executor>(num_threads);
}
