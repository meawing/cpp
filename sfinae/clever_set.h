#pragma once

#include <set>
#include <functional>
#include <unordered_set>
#include <cstddef>
#include <type_traits>

template <typename T>
class MyEqual {
public:
    bool operator()(const T& lhs, const T& rhs) const {
        return !(lhs != rhs);
    }
};

template <typename T>
class MyLess {
public:
    bool operator()(const T& lhs, const T& rhs) const {
        return rhs > lhs;
    }
};

template <class T>
struct CheckType {
    template <typename U>
    static auto CheckEqual(U* t) -> decltype(*t == *t, int()) {
        return 0;
    }

    template <typename U>
    static char CheckEqual(...) {
        return 0;
    }

    template <typename U>
    static auto CheckNotEqual(U* t) -> decltype(*t != *t, int()) {
        return 0;
    }

    template <typename U>
    static char CheckNotEqual(...) {
        return 0;
    }

    template <typename U>
    static auto CheckHash(U* t) -> decltype(std::hash<U>()(*t), int()) {
        return 0;
    }

    template <typename U>
    static char CheckHash(...) {
        return 0;
    }

    template <typename U>
    static auto CheckLess(U* t) -> decltype(*t < *t, int()) {
        return 0;
    }

    template <typename U>
    static char CheckLess(...) {
        return 0;
    }

    template <typename U>
    static auto CheckGreater(U* t) -> decltype(*t > *t, int()) {
        return 0;
    }

    template <typename U>
    static char CheckGreater(...) {
        return 0;
    }

    static const size_t kValue = sizeof(CheckHash<T>(nullptr)) == sizeof(int) &&
                                         sizeof(CheckEqual<T>(nullptr)) == sizeof(int)
                                     ? 0
                                 : sizeof(CheckHash<T>(nullptr)) == sizeof(int) &&
                                         sizeof(CheckNotEqual<T>(nullptr)) == sizeof(int)
                                     ? 1
                                 : sizeof(CheckLess<T>(nullptr)) == sizeof(int)    ? 2
                                 : sizeof(CheckGreater<T>(nullptr)) == sizeof(int) ? 3
                                                                                   : 4;
};

template <typename T, size_t = CheckType<T>::kValue>
class CleverSet;

template <typename T>
class CleverSet<T, 0> {
public:
    bool Insert(const T& value) {
        if (data_.find(value) != data_.end()) {
            return false;
        }
        data_.insert(value);
        return true;
    }

    bool Erase(const T& value) {
        if (data_.find(value) != data_.end()) {
            data_.erase(value);
            return true;
        } else {
            return false;
        }
    }

    bool Find(const T& value) const {
        if (data_.find(value) != data_.end()) {
            return true;
        } else {
            return false;
        }
    }

    size_t Size() const {
        return data_.size();
    }

private:
    std::unordered_set<T> data_;
};

template <typename T>
class CleverSet<T, 1> {
public:
    bool Insert(const T& value) {
        if (data_.find(value) != data_.end()) {
            return false;
        }
        data_.insert(value);
        return true;
    }

    bool Erase(const T& value) {
        if (data_.find(value) != data_.end()) {
            data_.erase(value);
            return true;
        } else {
            return false;
        }
    }

    bool Find(const T& value) const {
        if (data_.find(value) != data_.end()) {
            return true;
        } else {
            return false;
        }
    }

    size_t Size() const {
        return data_.size();
    }

private:
    std::unordered_set<T, std::hash<T>, MyEqual<T>> data_;
};

template <typename T>
class CleverSet<T, 2> {
public:
    bool Insert(const T& value) {
        if (data_.find(value) != data_.end()) {
            return false;
        }
        data_.insert(value);
        return true;
    }

    bool Erase(const T& value) {
        if (data_.find(value) != data_.end()) {
            data_.erase(value);
            return true;
        } else {
            return false;
        }
    }

    bool Find(const T& value) const {
        if (data_.find(value) != data_.end()) {
            return true;
        } else {
            return false;
        }
    }

    size_t Size() const {
        return data_.size();
    }

private:
    std::set<T> data_;
};

template <typename T>
class CleverSet<T, 3> {
public:
    bool Insert(const T& value) {
        if (data_.find(value) != data_.end()) {
            return false;
        }
        data_.insert(value);
        return true;
    }

    bool Erase(const T& value) {
        if (data_.find(value) != data_.end()) {
            data_.erase(value);
            return true;
        } else {
            return false;
        }
    }

    bool Find(const T& value) const {
        if (data_.find(value) != data_.end()) {
            return true;
        } else {
            return false;
        }
    }

    size_t Size() const {
        return data_.size();
    }

private:
    std::set<T, MyLess<T>> data_;
};

template <typename T>
class CleverSet<T, 4> {
public:
    bool Insert(const T& value) {
        if (data_.find(&value) != data_.end()) {
            return false;
        }
        data_.insert(&value);
        return true;
    }

    bool Erase(const T& value) {
        if (data_.find(&value) != data_.end()) {
            data_.erase(&value);
            return true;
        } else {
            return false;
        }
    }

    bool Find(const T& value) const {
        if (data_.find(&value) != data_.end()) {
            return true;
        } else {
            return false;
        }
    }

    size_t Size() const {
        return data_.size();
    }

private:
    std::unordered_set<const T*> data_;
};
