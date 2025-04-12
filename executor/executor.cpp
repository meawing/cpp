// executor.cpp
#include "executor.h"
#include <cassert>
#include <stdexcept>
#include <chrono>
#include <algorithm>

thread_local uint32_t current_thread_index = 0;

void Task::AddDependency(std::shared_ptr<Task> dep) {
    remaining_deps_.fetch_add(1, std::memory_order_relaxed);
    
    {
        std::lock_guard<std::mutex> lk(dep->dependents_mutex_);
        dep->dependents_.push_back({shared_from_this(), false});
    }
    
    if (dep->IsFinished()) {
        NotifyDependencyFinished();
    }
}

void Task::AddTrigger(std::shared_ptr<Task> trigger) {
    has_trigger_.store(true, std::memory_order_relaxed);
    
    {
        std::lock_guard<std::mutex> lk(trigger->dependents_mutex_);
        trigger->dependents_.push_back({shared_from_this(), true});
    }
}

void Task::SetTimeTrigger(std::chrono::system_clock::time_point at) {
    has_time_trigger_.store(true, std::memory_order_relaxed);
    time_trigger_ = at;
}

std::exception_ptr Task::GetError() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return error_;
}

void Task::Cancel() {
    TaskState::Value expected = TaskState::Pending;
    if (!state_.compare_exchange_strong(expected, TaskState::Canceled)) {
        return;
    }

    cv_.notify_all();

    std::vector<std::shared_ptr<Task>> to_notify;
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        to_notify.reserve(dependents_.size());
        for (auto& dep : dependents_) {
            if (auto t = dep.task.lock()) {
                to_notify.push_back(t);
            }
        }
        dependents_.clear();
    }

    for (auto& t : to_notify) {
        if (t) {
            if (t->remaining_deps_.load() > 0) {
                t->NotifyDependencyFinished();
            }
            t->NotifyTriggerFinished();
        }
    }
}

void Task::Wait() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [this] { return IsFinished(); });
}

void Task::RunTask() {
    TaskState::Value expected = TaskState::Pending;
    if (!state_.compare_exchange_strong(expected, TaskState::Running)) {
        return;
    }

    try {
        Run();
        state_.store(TaskState::Completed);
    } catch (...) {
        std::lock_guard<std::mutex> lk(mutex_);
        error_ = std::current_exception();
        state_.store(TaskState::Failed);
    }

    cv_.notify_all();

    std::vector<std::shared_ptr<Task>> to_notify;
    {
        std::lock_guard<std::mutex> lk(dependents_mutex_);
        to_notify.reserve(dependents_.size());
        for (auto& dep : dependents_) {
            if (auto t = dep.task.lock()) {
                if (dep.is_trigger) {
                    t->NotifyTriggerFinished();
                } else {
                    to_notify.push_back(t);
                }
            }
        }
        dependents_.clear();
    }

    for (auto& t : to_notify) {
        t->NotifyDependencyFinished();
    }
}

void Task::NotifyDependencyFinished() {
    if (remaining_deps_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this(), current_thread_index);
        }
    }
}

void Task::NotifyTriggerFinished() {
    bool expected = false;
    if (trigger_fired_.compare_exchange_strong(expected, true)) {
        if (auto exec = executor_.lock()) {
            exec->Enqueue(shared_from_this(), current_thread_index);
        }
    }
}

void Task::MarkFinished() {
    TaskState::Value expected = TaskState::Pending;
    state_.compare_exchange_strong(expected, TaskState::Canceled);
    cv_.notify_all();
}

Executor::Executor(uint32_t num_threads) : thread_queues_(num_threads) {
    workers_.reserve(num_threads);
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i] { 
            current_thread_index = i;
            WorkerLoop(i); 
        });
    }
    timer_thread_ = std::thread([this] { TimerThreadLoop(); });
}

Executor::~Executor() {
    StartShutdown();
    WaitShutdown();
}

void Executor::Submit(std::shared_ptr<Task> task) {
    task->executor_ = shared_from_this();

    const bool deps_ready = task->remaining_deps_.load() == 0;
    const bool trigger_ready = !task->has_trigger_.load() || task->trigger_fired_.load();

    if (task->has_time_trigger_.load()) {
        std::lock_guard<std::mutex> lk(timer_mutex_);
        timer_queue_.push({task, task->time_trigger_});
        timer_cv_.notify_one();
    } else if (deps_ready && trigger_ready) {
        Enqueue(task, next_thread_.fetch_add(1, std::memory_order_relaxed) % workers_.size());
    }
}

void Executor::StartShutdown() {
    shutdown_.store(true);
    {
        std::lock_guard<std::mutex> lk(timer_mutex_);
        timer_cv_.notify_all();
    }
    for (auto& q : thread_queues_) {
        std::lock_guard<std::mutex> lk(q.mutex);
    }
}

void Executor::WaitShutdown() {
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void Executor::Enqueue(std::shared_ptr<Task> task, uint32_t preferred_thread) {
    if (shutdown_.load()) {
        task->Cancel();
        return;
    }

    auto& queue = thread_queues_[preferred_thread % thread_queues_.size()];
    {
        std::lock_guard<std::mutex> lk(queue.mutex);
        queue.tasks.push_back(task);
    }
}

bool Executor::TryStealWork(std::shared_ptr<Task>& task, uint32_t thief_index) {
    const uint32_t num_queues = thread_queues_.size();
    for (uint32_t i = 1; i < num_queues; ++i) {
        uint32_t victim_index = (thief_index + i) % num_queues;
        auto& victim_queue = thread_queues_[victim_index];
        
        std::unique_lock<std::mutex> lk(victim_queue.mutex, std::try_to_lock);
        if (lk && !victim_queue.tasks.empty()) {
            task = victim_queue.tasks.front();
            victim_queue.tasks.pop_front();
            return true;
        }
    }
    return false;
}

void Executor::TimerThreadLoop() {
    while (!shutdown_.load()) {
        std::unique_lock<std::mutex> lock(timer_mutex_);
        
        if (timer_queue_.empty()) {
            timer_cv_.wait(lock, [this] {
                return !timer_queue_.empty() || shutdown_.load();
            });
        } else {
            auto next_trigger = timer_queue_.top().trigger_time;
            timer_cv_.wait_until(lock, next_trigger, [this, next_trigger] {
                return shutdown_.load() || 
                       (timer_queue_.empty() ? false : 
                        timer_queue_.top().trigger_time < next_trigger);
            });
        }
        
        if (shutdown_.load()) break;
        
        const auto now = std::chrono::system_clock::now();
        while (!timer_queue_.empty() && timer_queue_.top().trigger_time <= now) {
            auto task = timer_queue_.top().task;
            timer_queue_.pop();
            
            if (task->remaining_deps_.load() == 0) {
                Enqueue(task, next_thread_.fetch_add(1, std::memory_order_relaxed) % workers_.size());
            }
        }
    }
}

void Executor::WorkerLoop(uint32_t thread_index) {
    auto& local_queue = thread_queues_[thread_index];
    
    while (true) {
        std::shared_ptr<Task> task;
        
        // Try local queue first
        {
            std::lock_guard<std::mutex> lk(local_queue.mutex);
            if (!local_queue.tasks.empty()) {
                task = local_queue.tasks.front();
                local_queue.tasks.pop_front();
            }
        }

        if (!task && !TryStealWork(task, thread_index)) {
            if (shutdown_.load()) {
                break;
            }
            std::this_thread::yield();
            continue;
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