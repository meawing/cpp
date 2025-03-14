#pragma once

#include <cstdint>
#include <cstddef>

// Helper to compute power recursively at compile-time
constexpr int64_t PowMod(int64_t base, int exp, int mod) {
    return exp == 0 ? 1 : (base * PowMod(base, exp - 1, mod)) % mod;
}

// Primary template for compile-time string hashing
template <std::size_t N>
constexpr int Hash(const char (&s)[N], int p, int m, std::size_t i = 0, int hash = 0,
                   int64_t pow = 1) {
    return i == N - 1 ? hash : Hash(s, p, m, i + 1, (hash + (s[i] * pow) % m) % m, (pow * p) % m);
}

// Specialization for empty string
template <>
constexpr int Hash(const char (&s)[1], int p, int m, std::size_t i, int hash, int64_t pow) {
    return hash;
}
