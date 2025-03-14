#pragma once

#include <atomic>
#include <memory>

class Incrementer {
public:
    explicit Incrementer(std::atomic<int64_t>* counter) : counter_{counter} {
    }

    void Increment() {
        ++*counter_;
    }

private:
    std::atomic<int64_t>* counter_;
};

class ReadWriteAtomicCounter {
public:
    std::unique_ptr<Incrementer> GetIncrementer() {
        return std::make_unique<Incrementer>(&counter_);
    }

    int64_t GetValue() const {
        return counter_;
    }

private:
    std::atomic<int64_t> counter_ = 0;
};
