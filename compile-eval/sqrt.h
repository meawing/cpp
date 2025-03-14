#pragma once

#include <stdint.h>

template <int N, int L, int R, int D>
struct SqrtHelper {
    static const int kMid = (L + R) / 2;
    static const bool kMoveLeft = (static_cast<int64_t>(kMid) * kMid >= N);
    static const int kNextL = kMoveLeft ? L : kMid + 1;
    static const int kNextR = kMoveLeft ? kMid : R;
    static const int kNextD = (kNextR - kNextL) > 1 ? D : 1;  // reduce step when close to answer
    static const int kValue = SqrtHelper<N, kNextL, kNextR, kNextD>::kValue;
};

// terminal instance
template <int N, int M>
struct SqrtHelper<N, M, M, 1> {
    static const int kValue = M;
};

template <int N>
struct Sqrt {
    static const int kValue = SqrtHelper<N, 0, N, N / 2 + 1>::kValue;
};
