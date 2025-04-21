#include <catch2/catch_test_macros.hpp>

#include "intrusive_list.h"

using namespace cactus;

struct TestListTag {};

TEST_CASE("Link push back") {
    List<TestListTag> list;

    ListElement<TestListTag> a;
    ListElement<TestListTag> b;

    list.PushBack(&a);
    list.PushBack(&b);

    REQUIRE(list.Begin() == &a);
    REQUIRE(list.Back() == &b);
}
