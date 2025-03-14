#include "range_sum.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Basic tests") {
    CHECK(RangeSum(0, 0) == 0);
    CHECK(RangeSum(1, 1) == 0);
    CHECK(RangeSum(0, 5) == 10);
    CHECK(RangeSum(2, 6) == 14);
    CHECK(RangeSum(0, 3, 2) == 2);
    CHECK(RangeSum(5, 8, 2) == 12);
    CHECK(RangeSum(5, 9, 2) == 12);
    CHECK(RangeSum(0, 20, 3) == 63);
    CHECK(RangeSum(1, 10, 2) == 25);

    CHECK(RangeSum(5, 3) == 0);
    CHECK(RangeSum(5, 3, 2) == 0);
    CHECK(RangeSum(10, 9, 3) == 0);
}
