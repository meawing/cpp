#include "basic_parsers.h"
#include "basic_combinators.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PSkip") {
    using ps::PChar;
    using ps::PSkip;
    auto parser = PSkip{PChar<'a'>{}};
    static_assert(std::is_same_v<decltype(parser("")->val), ps::Unit>);
    REQUIRE(!parser("b").has_value());
    REQUIRE(!parser("").has_value());

    {
        std::string in = "a";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->tail.empty());
    }

    {
        std::string in = "abc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->tail == "bc");
    }
}

TEST_CASE("PMany") {
    using ps::PChar;
    using ps::PMany;
    using ps::literals::operator""_const;
    auto parser = PMany{PChar<'a'>{}, 1_const, 3_const};
    REQUIRE(!parser("bcx").has_value());
    {
        std::string in = "a";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == std::deque{'a'});
        REQUIRE(result->tail.empty());
    }

    {
        std::string in = "aabc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == std::deque{'a', 'a'});
        REQUIRE(result->tail == "bc");
    }

    {
        std::string in = "aaaabc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == std::deque{'a', 'a', 'a'});
        REQUIRE(result->tail == "abc");
    }
}

TEST_CASE("PMaybe") {
    using ps::PChar;
    using ps::PMaybe;

    auto parser = PMaybe{PChar<'a'>{}};

    {
        std::string in = "abc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val.has_value());
        REQUIRE(*result->val == 'a');
        REQUIRE(result->tail == "bc");
    }

    {
        std::string in = "bbc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(!result->val.has_value());
        REQUIRE(result->tail == "bbc");
    }
}

TEST_CASE("PApply") {
    auto parse_int = [](const std::deque<char>& d) -> std::expected<int, std::string> {
        int result = 0;
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
        return result;
    };

    using ps::PAnyChar;
    using ps::PApply;
    using ps::PMany;
    using ps::literals::operator""_const;

    auto parser = PMany(PAnyChar{}, 1_const, 3_const);
    auto int_parser = PApply(parse_int, PMany(PAnyChar{}, 1_const, 3_const));

    REQUIRE(parser("a").has_value());
    REQUIRE(!int_parser("a").has_value());
    REQUIRE(int_parser("123").has_value());
    REQUIRE(int_parser("123")->val == 123);
}
