#pragma once

constexpr int NextPrime(int x) {
    if (x <= 1) {
        return 2;
    }
    for (int y = x;; ++y) {
        bool is_prime = true;
        for (int i = 2; i * i <= y; ++i) {
            if (y % i == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) {
            return y;
        }
    }
}
