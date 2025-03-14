#pragma once

#include <unordered_map>
#include <mutex>

template <class K, class V, class Hash = std::hash<K>>
class ConcurrentHashMap {
public:
    explicit ConcurrentHashMap(const Hash& hasher = {})
        : ConcurrentHashMap{kUndefinedSize, hasher} {
    }

    explicit ConcurrentHashMap(int expected_size, const Hash& hasher = {})
        : ConcurrentHashMap{expected_size, kDefaultConcurrencyLevel, hasher} {
    }

    ConcurrentHashMap(int expected_size, int expected_threads_count, const Hash& hasher = {})
        :  // 0 because unordered_map doesn't contain constructor with hasher
          table_{0, hasher} {
        (void)expected_threads_count;
        if (expected_size != kUndefinedSize) {
            table_.reserve(expected_size);
        }
    }

    bool Insert(const K& key, const V& value) {
        std::lock_guard lock{mutex_};
        return table_.emplace(key, value).second;
    }

    bool Erase(const K& key) {
        std::lock_guard lock{mutex_};
        return table_.erase(key);
    }

    void Clear() {
        std::lock_guard lock{mutex_};
        table_.clear();
    }

    std::pair<bool, V> Find(const K& key) const {
        std::lock_guard lock{mutex_};
        if (auto it = table_.find(key); it != table_.end()) {
            return {true, it->second};
        } else {
            return {false, {}};
        }
    }

    V At(const K& key) const {
        std::lock_guard lock{mutex_};
        return table_.at(key);
    }

    size_t Size() const {
        std::lock_guard lock{mutex_};
        return table_.size();
    }

    static constexpr auto kUndefinedSize = -1;

private:
    static constexpr auto kDefaultConcurrencyLevel = 8;

    std::unordered_map<K, V, Hash> table_;
    mutable std::mutex mutex_;
};
