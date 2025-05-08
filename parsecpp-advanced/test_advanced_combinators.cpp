#include "advanced_combinators.h"
#include "basic_combinators.h"
#include "basic_parsers.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PChoiceUnique") {
    using ps::PChar;
    using ps::PChoice;

    auto parser = PChoice{PChar<'a'>{}, PChar<'b'>{}};
    static_assert(std::is_same_v<decltype(parser("asdf")->val), std::variant<char>>);
}

TEST_CASE("PChoice") {
    using ps::PChar;
    using ps::PChoice;

    auto parser = PChoice{PChar<'a'>{}, PChar<'b'>{}};
    REQUIRE(!parser("").has_value());
    REQUIRE(!parser("de").has_value());
    {
        std::string in = "abc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(std::get<char>(result->val) == 'a');
        REQUIRE(result->tail == "bc");
    }

    {
        std::string in = "babc";
        auto result = parser(in);
        REQUIRE(result.has_value());
        REQUIRE(std::get<char>(result->val) == 'b');
        REQUIRE(result->tail == "abc");
    }
}

TEST_CASE("PSeq skip units") {
    using ps::PAnyChar;
    using ps::PEof;
    using ps::PSeq;

    auto parser = PSeq(PAnyChar{}, PEof{}, PAnyChar{}, PEof{});
    static_assert(std::is_same_v<decltype(parser("")->val), std::tuple<char, char>>);
}

TEST_CASE("PSeq at least one unit or empty") {
    using ps::PEof;
    using ps::PSeq;

    auto parser = PSeq(PEof{}, PEof{});
    static_assert(std::is_same_v<decltype(parser("")->val), std::tuple<ps::Unit>>
            || std::is_same_v<decltype(parser("")->val), std::tuple<>>);
}

TEST_CASE("PSeq") {
    using ps::PChar;
    using ps::PSeq;
    using ps::PSkip;

    auto parser =
        ps::PSeq(PChar<'a'>{}, PChar<'b'>{}, PChar<'c'>{}, PSkip{PChar<'d'>{}}, PChar<'e'>{});

    REQUIRE(!parser("abcdd").has_value());
    auto result = parser("abcdef");
    REQUIRE(result.has_value());
    REQUIRE(result->val == std::tuple{'a', 'b', 'c', 'e'});
    REQUIRE(result->tail == "f");
}
