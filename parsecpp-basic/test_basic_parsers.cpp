#include "basic_parsers.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Eof") {
    using ps::PEof;
    auto parser = PEof{};
    REQUIRE(!parser("123").has_value());
    REQUIRE(parser("").has_value());
    REQUIRE(parser("")->tail.empty());
    static_assert(std::is_same_v<decltype(parser("")->val), ps::Unit>);
}

TEST_CASE("AnyChar") {
    using ps::PAnyChar;
    auto parser = PAnyChar{};

    REQUIRE(!parser("").has_value());

    {
        std::string in = "a";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == 'a');
        REQUIRE(result->tail.empty());
    }

    {
        std::string in = "abc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == 'a');
        REQUIRE(result->tail == "bc");
    }
}

TEST_CASE("PChar") {
    using ps::PChar;

    auto parser = PChar<'a'>{};
    REQUIRE(!parser("").has_value());
    REQUIRE(!parser("bca").has_value());
    REQUIRE(!parser("b").has_value());

    {
        std::string in = "a";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == 'a');
        REQUIRE(result->tail.empty());
    }

    {
        std::string in = "abc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == 'a');
        REQUIRE(result->tail == "bc");
    }
}

TEST_CASE("PString") {
    using ps::PString;
    auto parser = PString<'h', 'e', 'l', 'l', 'o'>{};
    REQUIRE(!parser("world").has_value());

    {
        std::string in = "hello";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == "hello");
        REQUIRE(result->tail.empty());
    }

    {
        std::string in = "hello world";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == "hello");
        REQUIRE(result->tail == " world");
    }
}

TEST_CASE("PSomeChar") {
    using ps::PSomeChar;
    auto parser = PSomeChar<'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'>{};

    REQUIRE(!parser("").has_value());
    REQUIRE(!parser("a").has_value());

    {
        std::string in = "123";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(result->val == '1');
        REQUIRE(result->tail == "23");
    }
}
