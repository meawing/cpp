#include "basic_combinators.h"
#include "basic_parsers.h"
#include "operators.h"

#include <type_traits>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PChoice") {
    using ps::PAnyChar;
    using ps::PChoice;
    using ps::PEof;
    using ps::PMaybe;

    using ps::operators::operator|;

    auto a = PAnyChar{};
    auto b = PMaybe{a};
    auto c = PEof{};

    auto p0 = a | b | c;
    auto p1 = PChoice(a, b, c);

    static_assert(std::is_same_v<decltype(p0), decltype(p1)>);
}

TEST_CASE("PSeq") {
    using ps::PAnyChar;
    using ps::PEof;
    using ps::PMaybe;
    using ps::PSeq;

    using ps::operators::operator+;

    auto a = PAnyChar{};
    auto b = PMaybe{a};
    auto c = PEof{};

    auto p0 = a + b + c;
    auto p1 = PSeq(a, b, c);

    static_assert(std::is_same_v<decltype(p0), decltype(p1)>);
}
