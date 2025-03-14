#pragma once

#include <iostream>
#include <array>
#include <stdexcept>

template <typename K, typename V, int MaxSize = 8>
class ConstexprMap {
public:
    constexpr ConstexprMap() : size_(0) {
    }
    constexpr V& operator[](const K& key) {
        if (size_ == MaxSize) {
            throw std::out_of_range("ConstexprMap is full");
        }
        size_t idx = FindIndex(key);
        if (idx == MaxSize + 1) {
            map_[size_] = {key, V()};
            return map_[size_++].second;
        } else {
            return map_[idx].second;
        }
    }

    constexpr const V& operator[](const K& key) const {
        size_t idx = FindIndex(key);
        if (size_ == MaxSize + 1) {
            throw std::out_of_range("Key not found");
        }
        return map_[idx].second;
    }

    constexpr bool Erase(const K& key) {
        size_t idx = FindIndex(key);
        for (size_t i = idx; i + 1 < size_; ++i) {
            map_[i] = map_[i + 1];
        }
        --size_;
        return true;
    }

    constexpr bool Find(const K& key) const {
        return FindIndex(key) < MaxSize;
    }

    constexpr size_t Size() const {
        return size_;
    }

    constexpr std::pair<K, V>& GetByIndex(size_t pos) {
        return map_[pos];
    }

    constexpr const std::pair<K, V>& GetByIndex(size_t pos) const {
        return map_[pos];
    }

private:
    constexpr size_t FindIndex(const K& key) const {
        for (size_t i = 0; i < size_; ++i) {
            if (map_[i].first == key) {
                return i;
            }
        }
        return MaxSize + 1;  //?
    }

    std::array<std::pair<K, V>, MaxSize> map_;
    size_t size_;
};
