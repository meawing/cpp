#pragma once

constexpr int Pow(int a, int b) {
    return (b == 0) ? 1 : a * Pow(a, b - 1);
}
