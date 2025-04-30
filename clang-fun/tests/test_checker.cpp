#include "../check_names.h"
#include "common.h"
#include "util.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Checker") {
    auto dir = GetFileDir(__FILE__).parent_path() / "checker";
    auto files = GetCppFiles(dir);
    REQUIRE_FALSE(files.empty());

    std::vector args = {"./test_check_names", "-p", "."};
    std::unordered_map<std::string, Statistics> expected;
    for (const auto& file : files) {
        args.push_back(file.c_str());
        expected.emplace(file.filename(), Statistics{});
    }

    auto result = CheckNames(args.size(), args.data());
    CHECK(result == expected);
}
