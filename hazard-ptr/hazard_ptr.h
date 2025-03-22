#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <memory>

extern std::shared_mutex lock;

void RegisterThread();
void UnregisterThread();

template <class T>
T* Acquire(std::atomic<T*>* ptr) {
    lock.lock_shared();
    return ptr->load();
}

inline void Release() {
    lock.unlock_shared();
}

template <class T, class Deleter = std::default_delete<T>>
void Retire(T* value, Deleter deleter = {}) {
    std::lock_guard guard{lock};
    deleter(value);
}
