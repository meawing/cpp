#pragma once

#include <mutex>
#include <atomic>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>

// Atomically do the following:
//    if (*value == expected_value) {
//        sleep_on_address(value)
//    }
inline void FutexWait(int* value, int expected_value) {
    syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expected_value, nullptr, nullptr, 0);
}

// Wakeup 'count' threads sleeping on address of value (-1 wakes all)
inline void FutexWake(int* value, int count) {
    syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

class Mutex {
public:
    void Lock() {
        mutex_.lock();
    }

    void Unlock() {
        mutex_.unlock();
    }

private:
    std::mutex mutex_;
};
