#include "resp_writer.h"

#include <vector>
#include <list>
#include <ranges>

#include <cactus/io/writer.h>

#include <catch2/catch_test_macros.hpp>

namespace {

struct TestWriter {
    cactus::StringWriter string_writer;
    redis::RespWriter writer{&string_writer};

    void Check(std::string_view expected) const {
        CHECK(string_writer.View() == expected);
    }
};

}  // namespace

TEST_CASE_METHOD(TestWriter, "SimpleString") {
    writer.WriteSimpleString("Simple string");
    Check("+Simple string\r\n");
}

TEST_CASE_METHOD(TestWriter, "Error") {
    writer.WriteError("ERR Some error");
    Check("-ERR Some error\r\n");
}

TEST_CASE_METHOD(TestWriter, "Int") {
    writer.WriteInt(-1099);
    Check(":-1099\r\n");
}

TEST_CASE_METHOD(TestWriter, "BulkString") {
    writer.WriteBulkString("Bulk\r\nstring");
    Check("$12\r\nBulk\r\nstring\r\n");
}

TEST_CASE_METHOD(TestWriter, "NullBulkString") {
    writer.WriteNullBulkString();
    Check("$-1\r\n");
}

TEST_CASE_METHOD(TestWriter, "Array") {
    writer.StartArray(3);
    writer.WriteSimpleString("blah");
    writer.WriteInt(90);
    writer.WriteError("ERR 404");
    Check(
        "*3\r\n"
        "+blah\r\n"
        ":90\r\n"
        "-ERR 404\r\n");
}

TEST_CASE_METHOD(TestWriter, "NullArray") {
    writer.WriteNullArray();
    Check("*-1\r\n");
}

TEST_CASE_METHOD(TestWriter, "ArrayOfInts") {
    SECTION("vector") {
        writer.WriteArrayInts(std::vector{1, 2, 3});
        Check("*3\r\n:1\r\n:2\r\n:3\r\n");
    }

    SECTION("array") {
        using A = int[];
        writer.WriteArrayInts(A{4, 5});
        Check("*2\r\n:4\r\n:5\r\n");
    }

    SECTION("list") {
        std::list list = {-3, 0};
        writer.WriteArrayInts(list);
        Check("*2\r\n:-3\r\n:0\r\n");
    }

    SECTION("filter") {
        auto f = [](int x) { return x % 2; };
        auto view = std::list{1, 2, 3, 3, 0, 4, 1} | std::views::filter(f);
        writer.WriteArrayInts(view);
        Check("*4\r\n:1\r\n:3\r\n:3\r\n:1\r\n");
    }

    SECTION("empty") {
        writer.WriteArrayInts(std::vector<int64_t>{});
        Check("*0\r\n");
    }
}
