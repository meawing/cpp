#pragma once

#include <array>

template <std::size_t N>
constexpr int Determinant(const std::array<std::array<int, N>, N>& a) {
    int det = 0;
    int sign = 1;
    for (std::size_t i = 0; i < N; ++i) {
        // Создаем матрицу-минор размером (N-1)x(N-1)
        std::array<std::array<int, N - 1>, N - 1> minor{};
        for (std::size_t row = 1; row < N; ++row) {
            std::size_t col_index = 0;
            for (std::size_t col = 0; col < N; ++col) {
                if (col == i) {
                    continue;
                }
                (&std::get<0>((&std::get<0>(minor))[row - 1]))[col_index] =
                    (&std::get<0>((&std::get<0>(a))[row]))[col];
                ++col_index;
            }
        }
        // Рекурсивно вычисляем определитель минора
        det += sign * a[0][i] * Determinant<N - 1>(minor);
        sign = -sign;  // Знак чередуется
    }
    return det;
}

template <>
constexpr int Determinant(const std::array<std::array<int, 1>, 1>& a) {
    return a[0][0];
}
