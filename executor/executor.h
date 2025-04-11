#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <functional>
#include <exception>

class Task : public std::enable_shared_from_this<Task> {
public:
    virtual ~Task() {
    }

    virtual void Run() = 0;

    void AddDependency(std::shared_ptr<Task> dep);
    void AddTrigger(std::shared_ptr<Task> dep);
    void SetTimeTrigger(std::chrono::system_clock::time_point at);

    // Task::Run() completed without throwing exception
    bool IsCompleted();

    // Task::Run() throwed exception
    bool IsFailed();

    // Task was Canceled
    bool IsCanceled();

    // Task either completed, failed or was Canceled
    bool IsFinished();

    std::exception_ptr GetError();

    void Cancel();

    void Wait();

private:
};

// Used instead of void in generic code
struct Unit {};

class Executor {
public:
    void Submit(std::shared_ptr<Task> task);

    void StartShutdown();
    void WaitShutdown();
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(uint32_t num_threads);
