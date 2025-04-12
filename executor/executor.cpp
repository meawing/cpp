// executor.cpp
#include "executor.h"
#include <cassert>
#include <stdexcept>
#include <chrono>

void Task::AddDependency(std::shared_ptr<Task> dep) {
    remaining_deps_.fetch_add(1, std::memory_order_relaxed);
    
    {
        std::lock_guard<std::mutex> lk(dep->dependents_mutex_);
        dep->dependents_.push_back(shared_from_this());
    }
    
    if (dep->IsFinished()) {
        NotifyDependencyFinished();
    }
}

void Task::AddTrigger(std::shared_ptr<Task> trigger) {
    has_trigger_.store(true, std::memory_order_relaxed);
    
    {
        std::lock_guard<std::mutex> lk(trigger->dependents_mutex_);
        trigger->trigger_dependents_.push_back(shared_from_this());
    }
}

void Task::SetTimeTrigger(std::chrono::system_clock::time_point at) {
    has_time_trigger_.store(true, std::memory_order_relaxed);
    time_trigger_ = at;
}

void Task::Cancel() {
    State expected = State::Pending;
    if (!state_.compare_exchange_strong(expected, State::Canceled)) {
        return;
    }

    cv_.notify_all();

    std::vector<std::shared_ptr<Task>> to_notify;
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        for (auto& weak_t : dependents_) {
            if (auto t = weak_t.lock()) to_notify.push_back(t);
        }
        dependents_.clear();

        for (auto& weak_t : trigger_dependents_) {
            if (auto t = weak_t.lock()) to_notify.push_back(t);
        }
        trigger_dependents_.clear();
    }

    for (auto& t : to_notify) {
        t->NotifyDependencyFinished();
        t->NotifyTriggerFinished();
    }
}

void Task::Wait() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [this] {
        return state_.load() != State::Pending && state_.load() != State::Running;
    });
}

void Task::RunTask() {
    State expected = State::Pending;
    if (!state_.compare_exchange_strong(expected, State::Running)) {
        return;
    }

    try {
        Run();
        state_.store(State::Completed, std::memory_order_release);
    } catch (...) {
        std::lock_guard<std::mutex> lk(mutex_);
        error_ = std::current_exception();
        state_.store(State::Failed, std::memory_order_release);
    }

    cv_.notify_all();

    std::vector<std::shared_ptr<Task>> to_notify;
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        for (auto& weak_t : dependents_) {
            if (auto t = weak_t.lock()) to_notify.push_back(t);
        }
        dependents_.clear();
    }

    for (auto& t : to_notify) {
        t->NotifyDependencyFinished();
    }

    to_notify.clear();
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        for (auto& weak_t : trigger_dependents_) {
            if (auto t = weak_t.lock()) to_notify.push_back(t);
        }
        trigger_dependents_.clear();
    }

    for (auto& t : to_notify) {
        t->NotifyTriggerFinished();
    }
}

void Task::NotifyDependencyFinished() {
    if (remaining_deps_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this());
        }
    }
}

void Task::NotifyTriggerFinished() {
    bool expected = false;
    if (trigger_fired_.compare_exchange_strong(expected, true)) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this());
        }
    }
}

void Task::MarkFinished() {
    State expected = State::Pending;
    state_.compare_exchange_strong(expected, State::Canceled);
    cv_.notify_all();
}

Executor::Executor(uint32_t num_threads) {
    workers_.reserve(num_threads);
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { WorkerLoop(); });
    }
}

Executor::~Executor() {
    StartShutdown();
    WaitShutdown();
}

void Executor::Submit(std::shared_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        if (shutdown_.load()) {
            task->Cancel();
            return;
        }
        task->executor_ = shared_from_this();
    }

    auto now = std::chrono::system_clock::now();
    bool ready = false;

    if (task->remaining_deps_.load(std::memory_order_relaxed) == 0) {
        if (!task->has_time_trigger_.load(std::memory_order_relaxed) || 
            (now >= task->time_trigger_)) {
            if (!task->has_trigger_.load(std::memory_order_relaxed) || 
                task->trigger_fired_.load(std::memory_order_relaxed)) {
                ready = true;
            }
        }
    }

    if (ready) {
        Enqueue(task);
    } else if (task->has_time_trigger_.load(std::memory_order_relaxed)) {
        auto deadline = task->time_trigger_;
        auto delay = deadline - now;
        std::thread([task, delay, this]() {
            std::this_thread::sleep_for(delay);
            if (task->remaining_deps_.load(std::memory_order_relaxed) == 0) {
                this->Enqueue(task);
            }
        }).detach();
    }
}

void Executor::StartShutdown() {
    shutdown_.store(true);
    queue_cv_.notify_all();
}

void Executor::WaitShutdown() {
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void Executor::Enqueue(std::shared_ptr<Task> task) {
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (shutdown_.load()) {
        task->Cancel();
        return;
    }
    ready_queue_.push_back(task);
    queue_cv_.notify_one();
}

void Executor::WorkerLoop() {
    while (true) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this] {
                return shutdown_.load() || !ready_queue_.empty();
            });
            
            if (shutdown_.load() && ready_queue_.empty()) break;
            
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
