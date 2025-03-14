#include "range_sum.h"

uint64_t RangeSum(uint64_t from, uint64_t to, uint64_t step) {
    if (from >= to) {
        return 0;
    }
    int n = (to - 1 - from) / step;
    return ((n + 1) * (2 * from + n * step)) / 2;
}
