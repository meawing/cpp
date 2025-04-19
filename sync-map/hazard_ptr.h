#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <thread>

void ScanFreeList();

// Thread-local hazard pointer
thread_local std::atomic<void*> hazard_ptr{nullptr};

// Per-thread state for tracking hazard pointers
class ThreadState {
public:
    ThreadState() : ptr(&hazard_ptr) {
    }
    std::atomic<void*>* ptr;
};

// Structure for retired pointers waiting to be deleted
struct RetiredPtr {
    void* value;
    std::function<void()> deleter;
    RetiredPtr* next;
};

// Global state
std::mutex threads_lock;
std::unordered_set<ThreadState*> threads;
std::atomic<RetiredPtr*> free_list{nullptr};
std::atomic<size_t> approximate_free_list_size{0};
std::mutex scan_lock;

// Threshold for triggering scan
const size_t kScanThreshold = 10;  // Lower threshold to ensure more frequent scans

void RegisterThread() {
    auto* state = new ThreadState();
    std::lock_guard guard{threads_lock};
    threads.insert(state);
}

void UnregisterThread() {
    // Clean up thread-local hazard_ptr
    hazard_ptr.store(nullptr);

    // Remove thread from global registry
    ThreadState* my_state = nullptr;
    {
        std::lock_guard guard{threads_lock};
        for (auto it = threads.begin(); it != threads.end(); ++it) {
            if ((*it)->ptr == &hazard_ptr) {
                my_state = *it;
                threads.erase(it);
                break;
            }
        }
    }

    // Scan free list before exiting to clean up any pointers this thread was protecting
    ScanFreeList();

    delete my_state;
}

template <class T>
T* Acquire(std::atomic<T*>* ptr) {
    T* value = ptr->load();

    // Handle null pointer case
    if (!value) {
        hazard_ptr.store(nullptr);
        return nullptr;
    }

    do {
        hazard_ptr.store(value);

        T* new_value = ptr->load();
        if (new_value == value) {
            return value;
        }

        value = new_value;
        // Handle null pointer case in the loop
        if (!value) {
            hazard_ptr.store(nullptr);
            return nullptr;
        }
    } while (true);
}

inline void Release() {
    hazard_ptr.store(nullptr);
}

void ScanFreeList() {
    // Reset approximate size to prevent multiple scans
    approximate_free_list_size.store(0);

    // Only one thread should scan at a time
    if (!scan_lock.try_lock()) {
        return;
    }

    // Use RAII to ensure mutex is unlocked
    std::unique_lock<std::mutex> guard(scan_lock, std::adopt_lock);

    // Take all retired pointers from the free list
    RetiredPtr* retired = free_list.exchange(nullptr);
    if (!retired) {
        return;  // Nothing to scan
    }

    // Collect all hazard pointers
    std::vector<void*> hazards;
    {
        std::lock_guard thread_guard{threads_lock};
        for (auto* thread : threads) {
            if (auto* ptr = thread->ptr->load()) {
                hazards.push_back(ptr);
            }
        }
    }

    // Sort hazards for binary search
    std::sort(hazards.begin(), hazards.end());

    // Process retired pointers
    RetiredPtr* to_keep_head = nullptr;
    RetiredPtr* to_keep_tail = nullptr;

    while (retired) {
        RetiredPtr* next = retired->next;

        // Check if this pointer is still hazardous
        bool is_hazardous =
            !hazards.empty() && std::binary_search(hazards.begin(), hazards.end(), retired->value);

        if (is_hazardous) {
            // Keep in the free list
            if (!to_keep_head) {
                to_keep_head = retired;
                to_keep_tail = retired;
            } else {
                to_keep_tail->next = retired;
                to_keep_tail = retired;
            }
            retired->next = nullptr;
        } else {
            // Safe to delete
            retired->deleter();
            delete retired;
        }

        retired = next;
    }

    // Put remaining hazardous pointers back in the free list
    if (to_keep_head) {
        // Use a loop to count and add to approximate_free_list_size
        size_t count = 0;
        RetiredPtr* current = to_keep_head;
        while (current) {
            count++;
            current = current->next;
        }

        // Push the entire list back
        RetiredPtr* old_head;
        do {
            old_head = free_list.load();
            to_keep_tail->next = old_head;
        } while (!free_list.compare_exchange_weak(old_head, to_keep_head));

        approximate_free_list_size.fetch_add(count);
    }
}

template <class T, class Deleter = std::default_delete<T>>
void Retire(T* value, Deleter deleter = {}) {
    if (!value) {
        return;
    }

    // Create a new retired pointer
    auto* retired = new RetiredPtr;
    retired->value = value;
    retired->deleter = [value, deleter]() { deleter(value); };
    retired->next = nullptr;

    // Add to free list using lock-free push
    RetiredPtr* old_head;
    do {
        old_head = free_list.load();
        retired->next = old_head;
    } while (!free_list.compare_exchange_weak(old_head, retired));

    // Increment approximate size and check if we need to scan
    if (approximate_free_list_size.fetch_add(1) >= kScanThreshold) {
        ScanFreeList();
    }
}
