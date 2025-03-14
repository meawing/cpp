#pragma once

#include "concurrent_hash_map.h"

#include <random>
#include <thread>
#include <vector>
#include <limits>
#include <unordered_set>
#include <forward_list>

#include <catch2/catch_test_macros.hpp>

enum class QueryType { INSERT, ERASE, FIND, CLEAR };

class Increment {
public:
    explicit Increment(int start) : start_{start} {
    }
    int operator()() {
        return start_++;
    }

private:
    int start_;
};

class EqualLowBits {
public:
    explicit EqualLowBits(int low_bits_count) : low_bits_count_{low_bits_count} {
    }
    int operator()() {
        auto result = ((start_++) << low_bits_count_) + low_bits_;
        if (start_ == (1 << (32 - low_bits_count_))) {
            start_ = 0;
            ++low_bits_;
        }
        return result;
    }

private:
    int start_ = 0;
    int low_bits_ = 0;
    const int low_bits_count_;
};

class Random {
public:
    explicit Random(uint32_t seed, int min = std::numeric_limits<int>::min(),
                    int max = std::numeric_limits<int>::max())
        : gen_{seed}, dist_{min, max} {
    }
    int operator()() {
        return dist_(gen_);
    }

private:
    std::mt19937 gen_;
    std::uniform_int_distribution<int> dist_;
};

template <bool check_insertions = false>
class Tester {
public:
    explicit Tester(uint32_t num_iterations = kMaxIterations) : num_iterations_{num_iterations} {
    }
    ~Tester() {
        threads_.clear();
        if constexpr (!check_insertions) {
            return;
        }
        std::unordered_set<int> set;
        for (const auto& logger : loggers_) {
            for (auto x : logger) {
                REQUIRE(table_.Find(x).first);
                set.insert(x);
            }
        }
        REQUIRE(table_.Size() == set.size());
    }

    template <QueryType type>
    void AddTask(auto gen) {
        std::vector<int>* logger{};
        if constexpr (check_insertions) {
            if constexpr (type == QueryType::INSERT) {
                logger = &loggers_.emplace_front();
                logger->reserve(num_iterations_);
            } else {
                static_assert(type == QueryType::FIND);
            }
        }

        auto task = [this, logger, gen = std::move(gen)]() mutable {
            for (auto i = 0u; i < num_iterations_; ++i) {
                auto value = gen();

                if constexpr (check_insertions && (type == QueryType::INSERT)) {
                    logger->push_back(value);
                } else {
                    (void)logger;
                }

                if constexpr (type == QueryType::INSERT) {
                    table_.Insert(value, 1);
                } else if constexpr (type == QueryType::ERASE) {
                    table_.Erase(value);
                } else if constexpr (type == QueryType::FIND) {
                    table_.Find(value);
                } else if constexpr (type == QueryType::CLEAR) {
                    table_.Clear();
                }
            }
        };

        threads_.emplace_back(std::move(task));
    }

    static constexpr auto kMaxIterations = 10'000u;

private:
    const uint32_t num_iterations_;
    ConcurrentHashMap<int, int> table_;
    std::forward_list<std::vector<int>> loggers_;
    std::vector<std::jthread> threads_;
};
