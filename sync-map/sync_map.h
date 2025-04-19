#pragma once

#include "../hazard-ptr/hazard_ptr.h"

template <class K, class V>
class SyncMap {
public:
    bool Lookup(const K& key, V* value);
    bool Insert(const K& key, const V& value);
};
