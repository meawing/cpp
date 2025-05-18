#include "common.h"
#include "util.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Dict") {
    auto dir = GetFileDir(__FILE__) / "dict";
    CheckDir(dir , dir / "dict.txt");
}
