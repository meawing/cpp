#pragma once

#include <numeric>
#include <iterator>
#include <thread>
#include <vector>
#include <future>

template <std::random_access_iterator Iterator, class T>
T Reduce(Iterator first, Iterator last, const T& init, auto func) {
    // Get the number of elements
    size_t size = std::distance(first, last);

    // For very small arrays, use standard reduction
    if (size <= 1000) {
        return std::reduce(first, last, init, func);
    }

    // Determine number of threads based on hardware
    int num_threads = std::thread::hardware_concurrency();
    // Use at least 2 threads, but not more than needed
    num_threads = std::max(2, std::min(num_threads, static_cast<int>(size / 100)));

    std::vector<std::future<T>> futures(num_threads);
    std::vector<T> results(num_threads);

    // Divide work among threads
    size_t chunk_size = size / num_threads;

    // Launch worker threads
    for (int i = 0; i < num_threads; ++i) {
        Iterator chunk_start = first + i * chunk_size;
        Iterator chunk_end = (i == num_threads - 1) ? last : chunk_start + chunk_size;

        futures[i] = std::async(std::launch::async, [=, &func]() {
            if (chunk_start == chunk_end) {
                // Empty chunk
                return init;
            }
            // Use the STL reduce for each chunk
            return std::reduce(chunk_start + 1, chunk_end, *chunk_start, func);
        });
    }

    // Collect results
    for (int i = 0; i < num_threads; ++i) {
        results[i] = futures[i].get();
    }

    // Final reduction of the partial results
    return std::reduce(results.begin(), results.end(), init, func);
}
