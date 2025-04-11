// executor.cpp
#include "executor.h"
#include <cassert>
#include <stdexcept>

// =======================
// Implementation of Task
// =======================

void Task::AddDependency(std::shared_ptr<Task> dep) {
    // Lock to protect the dependencies vector.
    std::lock_guard<std::mutex> lk(mutex_);
    dependencies_.push_back(dep);
}

void Task::AddTrigger(std::shared_ptr<Task> dep) {
    std::lock_guard<std::mutex> lk(mutex_);
    triggers_.push_back(dep);
}

void Task::SetTimeTrigger(std::chrono::system_clock::time_point at) {
    std::lock_guard<std::mutex> lk(mutex_);
    time_trigger_ = at;
    has_time_trigger_ = true;
}

bool Task::IsCompleted() {
    return state_.load(std::memory_order_acquire) == State::Completed;
}

bool Task::IsFailed() {
    return state_.load(std::memory_order_acquire) == State::Failed;
}

bool Task::IsCanceled() {
    return state_.load(std::memory_order_acquire) == State::Canceled;
}

bool Task::IsFinished() {
    auto s = state_.load(std::memory_order_acquire);
    return (s == State::Completed ||
            s == State::Failed ||
            s == State::Canceled);
}

std::exception_ptr Task::GetError() {
    std::lock_guard<std::mutex> lk(mutex_);
    return error_;
}

void Task::Cancel() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (state_.load(std::memory_order_acquire) == State::Pending) {
        state_.store(State::Canceled, std::memory_order_release);
        cv_.notify_all();
    }
}

void Task::Wait() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [this] {
        return state_ != State::Pending && state_ != State::Running;
    });
}

bool Task::IsReady() {
    std::vector<std::weak_ptr<Task>> local_dependencies;
    std::vector<std::weak_ptr<Task>> local_triggers;
    bool has_time = false;
    std::chrono::system_clock::time_point time_trigger;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        // the task is ready if it is not pending (or running) anyway.
        if (state_.load(std::memory_order_acquire) != State::Pending)
            return true;
        has_time = has_time_trigger_;
        time_trigger = time_trigger_;
        local_dependencies = dependencies_;
        local_triggers = triggers_;
    }

    auto now = std::chrono::system_clock::now();
    bool time_ok = has_time && (now >= time_trigger);

    // Check triggers: if at least one trigger is finished.
    bool trigger_ok = false;
    for (auto& w : local_triggers) {
        if (auto t = w.lock()) {
            if (t->IsFinished()) {
                trigger_ok = true;
                break;
            }
        }
    }

    // Check dependencies: if there are any, then ALL must be finished.
    bool dep_ok = false;
    if (!local_dependencies.empty()) {
        dep_ok = true;
        for (auto& w : local_dependencies) {
            if (auto dep = w.lock()) {
                if (!dep->IsFinished()) {
                    dep_ok = false;
                    break;
                }
            }
            // If the dependency pointer expired, assume it is finished.
        }
    }

    // If any conditions were added, then the task becomes ready only if at least one condition is met.
    if (!local_dependencies.empty() || !local_triggers.empty() || has_time) {
        return dep_ok || trigger_ok || time_ok;
    }
    return true;
}

void Task::RunTask() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (state_ != State::Pending)
            return; // if already canceled, do nothing
        state_ = State::Running;
    }
    try {
        Run();
        {
            std::lock_guard<std::mutex> lk(mutex_);
            state_ = State::Completed;
            cv_.notify_all();
        }
    } catch(...) {
        std::lock_guard<std::mutex> lk(mutex_);
        error_ = std::current_exception();
        state_ = State::Failed;
        cv_.notify_all();
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

void Task::MarkFinished() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (state_ == State::Pending)
        state_ = State::Canceled;
    cv_.notify_all();
}

// ==========================
// Implementation of Executor
// ==========================

Executor::Executor(uint32_t num_threads) {
    // Launch a fixed number of threads. They are all started in the constructor.
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
        std::lock_guard<std::mutex> lk(mtx_);
        // If shutdown is active, cancel this task immediately.
        if (shutdown_) {
            task->Cancel();
            return;
        }
        // Place the task in the pending list.
        pending_.push_back(task);
    }
    cv_.notify_all();
}

void Executor::StartShutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        shutdown_ = true;
        // Cancel any tasks that haven’t started.
        for (auto& t : pending_) {
            t->Cancel();
        }
    }
    cv_.notify_all();
}

void Executor::WaitShutdown() {
    // Wait for all worker threads to exit.
    for (auto& thread : workers_) {
        if (thread.joinable())
            thread.join();
    }
}

void Executor::WorkerLoop() {
    while (true) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lk(mtx_);

            // Continuously check if any tasks are ready.
            // First, try to promote pending tasks that meet their conditions.
            cv_.wait(lk, [this] {
                return shutdown_ || !pending_.empty() || !ready_.empty();
            });

            // Scan through pending tasks – if the task is either canceled or its ready conditions are met, move it to ready_.
            auto now = std::chrono::system_clock::now();
            for (auto it = pending_.begin(); it != pending_.end(); ) {
                if ((*it)->IsCanceled() || (*it)->IsReady()) {
                    ready_.push_front(*it);  // <<== Push ready tasks at the front.
                    it = pending_.erase(it);
                } else {
                    ++it;
                }
            }

            // If there is at least one ready task, pop one.
            if (!ready_.empty()) {
                task = ready_.front();
                ready_.pop_front();
            }
            else {
                // If there is no ready task, then either:
                // (a) no pending tasks, or (b) some tasks with time triggers not yet expired.
                if (shutdown_ && pending_.empty() && ready_.empty())
                    break; // exit thread if shutdown and no more tasks

                // Determine the nearest deadline from tasks with time triggers.
                std::chrono::system_clock::time_point next_time = now + std::chrono::hours(24);
                bool found_deadline = false;
                for (auto& t : pending_) {
                    if (t->HasTimeTrigger()) {
                        auto tt = t->GetTimeTrigger();
                        if (tt < next_time) {
                            next_time = tt;
                            found_deadline = true;
                        }
                    }
                }
                if (found_deadline) {
                    cv_.wait_until(lk, next_time);
                } else {
                    // Wait a short time so that we recheck conditions.
                    cv_.wait_for(lk, std::chrono::milliseconds(1));
                }
                continue; // then re-loop to re-check ready tasks.
            }
        } // release executor lock

        // Now run the task if not canceled.
        if (task) {
            if (task->IsCanceled()) {
                task->MarkFinished(); // ensure waiters are notified
            } else {
                task->RunTask();
            }
            // After finishing a task, notify all waiting (this might allow other tasks depending on this one
            // to become ready).
            {
                std::lock_guard<std::mutex> lk(mtx_);
                cv_.notify_all();
            }
        }
    }
}

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads) {
    return std::make_shared<Executor>(num_threads);
}
