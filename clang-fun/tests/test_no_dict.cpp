#include "common.h"
#include "util.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("NoDict") {
    CheckDir(GetFileDir(__FILE__) / "no-dict");
}
