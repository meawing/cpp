#include "is_prime.h"

#include <cmath>
#include <algorithm>
#include <ranges>

bool IsPrime(uint64_t x) {
    if (x < 2) {
        return false;
    }
    auto root = static_cast<uint64_t>(std::sqrt(x));
    auto bound = std::min(root + 6, x);
    for (auto i : std::views::iota(uint64_t{2}, bound)) {
        if (x % i == 0) {
            return false;
        }
    }
    return true;
}
