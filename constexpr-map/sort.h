#pragma once

#include "constexpr_map.h"

template <class K, class V, int S>
constexpr auto Sort(ConstexprMap<K, V, S> map) {
    size_t size = map.Size() - 1;
    for (size_t step = 0; step < size; ++step) {
        for (size_t i = 0; i < size - step; ++i) {
            if constexpr (std::is_same_v<K, int>) {
                if (map.GetByIndex(i).first < map.GetByIndex(i + 1).first) {
                    auto temp = map.GetByIndex(i);
                    map.GetByIndex(i) = map.GetByIndex(i + 1);
                    map.GetByIndex(i + 1) = temp;
                }
            } else {
                if (map.GetByIndex(i).first > map.GetByIndex(i + 1).first) {
                    auto temp = map.GetByIndex(i);
                    map.GetByIndex(i) = map.GetByIndex(i + 1);
                    map.GetByIndex(i + 1) = temp;
                }
            }
        }
    }
    return map;
}