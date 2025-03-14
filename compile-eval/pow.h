#pragma once

template <unsigned a, unsigned b>
struct Pow {
    static const unsigned kValue = a * Pow<a, b - 1>::kValue;
};

// Base case specialization to stop the recursion
template <unsigned a>
struct Pow<a, 0> {
    static const unsigned kValue = 1;
};
