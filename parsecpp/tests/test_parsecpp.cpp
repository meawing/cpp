#include "parsecpp.h"

#include <type_traits>
#include <string>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Parse int decl single char") {
    auto parse_int = [](const std::tuple<std::optional<char>, std::deque<char>>& vals)
        -> std::expected<int, std::string> {
        int result = 0;
        const auto& d = std::get<1>(vals);
        if (d.size() == 1 && (d[0] >= '0' && d[0] <= '9')) {
            result = d[0] - '0';
            return result;
        }
        for (char c : d) {
            if (c >= '0' && c <= '9') {
                if (result == 0 && c == '0') {
                    return std::unexpected("starts with zero");
                }
                result *= 10;
                result += (c - '0');
            } else {
                return std::unexpected("not a number");
            }
        }
        if (std::get<0>(vals).has_value()) {
            result *= -1;
        }

        return result;
    };

    using ps::literals::operator""_const;
    using ps::operators::operator+;
    using ps::operators::operator|;

    auto numeric = ps::PSomeChar<'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'>{};

    auto integer =
        ps::PApply(parse_int, ps::PMaybe(ps::PChar<'-'>{}) + ps::PMany(numeric, 1_const, 10_const));

    auto int_decl = ps::PString<'i', 'n', 't'>{};

    auto name = ps::PAnyChar{};

    auto space = ps::PChar<' '>{};
    auto spaces1 = ps::PSkip(ps::PMany(space, 1_const, 256_const));
    auto spaces0 = ps::PSkip(ps::PMany(space, 0_const, 256_const));

    auto line = ps::PSkip(int_decl) + spaces1 + name +
                ps::PMaybe(spaces1 + ps::PSkip(ps::PChar<'='>{}) + spaces0 + integer) +
                ps::PSkip(ps::PChar<';'>{});

    static_assert(sizeof(line) == 1);

    {
        std::string input = "int x = 10;";
        auto result = line(input);
        REQUIRE(result.has_value());
        REQUIRE(result->tail.empty());
        REQUIRE(std::get<0>(result->val) == 'x');
        REQUIRE(std::get<1>(result->val).has_value());
        REQUIRE(std::get<0>(*std::get<1>(result->val)) == 10);
    }

    {
        std::string input = "int x = 10";
        auto result = line(input);
        REQUIRE(!result.has_value());
    }

    {
        std::string input = "int x = -10;";
        auto result = line(input);
        REQUIRE(result.has_value());
        REQUIRE(result->tail.empty());
        REQUIRE(std::get<0>(result->val) == 'x');
        REQUIRE(std::get<1>(result->val).has_value());
        REQUIRE(std::get<0>(*std::get<1>(result->val)) == -10);
    }

    {
        std::string input = "int x =-10;";
        auto result = line(input);
        REQUIRE(result.has_value());
        REQUIRE(result->tail.empty());
        REQUIRE(std::get<0>(result->val) == 'x');
        REQUIRE(std::get<1>(result->val).has_value());
        REQUIRE(std::get<0>(*std::get<1>(result->val)) == -10);
    }

    {
        std::string input = "int x;";
        auto result = line(input);
        REQUIRE(result.has_value());
        REQUIRE(result->tail.empty());
        REQUIRE(std::get<0>(result->val) == 'x');
        REQUIRE(!std::get<1>(result->val).has_value());
    }
}
