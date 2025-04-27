#include <cactus/io/view.h>

#undef CHECK

#include <catch2/catch_test_macros.hpp>

using namespace cactus;

TEST_CASE("ConstView") {
    int x[1];
    View(x);
}
