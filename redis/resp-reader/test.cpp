#include "resp_reader.h"
#include "util.h"

#include <cstring>
#include <deque>
#include <vector>
#include <functional>

#include <cactus/io/reader.h>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr auto kArray = &redis::RespReader::ReadArrayLength;
constexpr auto kBulk = &redis::RespReader::ReadBulkString;
constexpr auto kError = &redis::RespReader::ReadError;
constexpr auto kInt = &redis::RespReader::ReadInt;
constexpr auto kString = &redis::RespReader::ReadSimpleString;
constexpr auto kType = &redis::RespReader::ReadType;

class ReaderTest {
public:
    static cactus::ViewReader MakeViewReader(const char* data) {
        return cactus::ViewReader{cactus::View(data, std::strlen(data))};
    }

    static cactus::LoopReader MakeLoopReader(const char* data) {
        return cactus::LoopReader{cactus::View(data, std::strlen(data))};
    }

    redis::RespReader MakeReader(const char* data) {
        auto& view = views_.emplace_back(MakeViewReader(data));
        return redis::RespReader{&view};
    }

    template <auto... methods>
    void CheckThrows(const char* data) {
        auto check_throws = [&](auto method) {
            auto reader = MakeReader(data);
            CHECK_THROWS_AS((reader.*method)(), redis::RedisError);
        };
        (check_throws(methods), ...);
    }

    void CheckThrowsAll(const char* data) {
        CheckThrows<kArray, kBulk, kError, kInt, kString, kType>(data);
    }

private:
    std::deque<cactus::ViewReader> views_;

    // Protection from OOM
    MemoryGuard guard_ = MakeMemoryGuard(10'000'000);
};

}  // namespace

TEST_CASE_METHOD(ReaderTest, "SimpleString") {
    auto reader = MakeReader("+OK\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::SimpleString);
    REQUIRE(reader.ReadSimpleString() == "OK");
}

TEST_CASE_METHOD(ReaderTest, "Error") {
    auto reader = MakeReader("-ERR Some error\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::Error);
    REQUIRE(reader.ReadError() == "ERR Some error");
}

TEST_CASE_METHOD(ReaderTest, "Int") {
    auto reader = MakeReader(":42\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::Int);
    REQUIRE(reader.ReadInt() == 42);
}

TEST_CASE_METHOD(ReaderTest, "BulkString") {
    auto reader = MakeReader("$11\r\nBulk string\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
    REQUIRE(reader.ReadBulkString() == "Bulk string");
}

TEST_CASE_METHOD(ReaderTest, "BulkStringWithNewLine") {
    auto reader = MakeReader("$5\r\nab\r\na\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
    REQUIRE(reader.ReadBulkString() == "ab\r\na");
}

TEST_CASE_METHOD(ReaderTest, "NullBulkString") {
    auto reader = MakeReader("$-1\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
    REQUIRE_FALSE(reader.ReadBulkString().has_value());
}

TEST_CASE_METHOD(ReaderTest, "Array") {
    auto reader = MakeReader(
        "*2\r\n"
        ":173\r\n"
        "$3\r\nYES\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::Array);
    REQUIRE(reader.ReadArrayLength() == 2);
    REQUIRE(reader.ReadType() == redis::ERespType::Int);
    REQUIRE(reader.ReadInt() == 173);
    REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
    REQUIRE(reader.ReadBulkString() == "YES");
}

TEST_CASE_METHOD(ReaderTest, "NullArray") {
    auto reader = MakeReader("*-1\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::Array);
    REQUIRE(reader.ReadArrayLength() == -1);
}

TEST_CASE_METHOD(ReaderTest, "ArrayInsideArray") {
    auto reader = MakeReader(
        "*3\r\n"
        "*0\r\n"
        "*3\r\n"
        ":-15\r\n"
        "-WRONG: wrong type\r\n"
        "*-1\r\n"
        "$9\r\nsomething\r\n"
        "+foobar\r\n");

    REQUIRE(reader.ReadType() == redis::ERespType::Array);
    REQUIRE(reader.ReadArrayLength() == 3);
    {
        REQUIRE(reader.ReadType() == redis::ERespType::Array);
        REQUIRE(reader.ReadArrayLength() == 0);

        REQUIRE(reader.ReadType() == redis::ERespType::Array);
        REQUIRE(reader.ReadArrayLength() == 3);
        {
            REQUIRE(reader.ReadType() == redis::ERespType::Int);
            REQUIRE(reader.ReadInt() == -15);

            REQUIRE(reader.ReadType() == redis::ERespType::Error);
            REQUIRE(reader.ReadError() == "WRONG: wrong type");

            REQUIRE(reader.ReadType() == redis::ERespType::Array);
            REQUIRE(reader.ReadArrayLength() == -1);
        }

        REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
        REQUIRE(reader.ReadBulkString() == "something");
    }

    REQUIRE(reader.ReadType() == redis::ERespType::SimpleString);
    REQUIRE(reader.ReadSimpleString() == "foobar");
}

TEST_CASE_METHOD(ReaderTest, "IsStreaming") {
    auto view_reader = MakeViewReader(":-6\r\n");
    redis::RespReader reader{&view_reader};

    REQUIRE(reader.ReadType() == redis::ERespType::Int);
    REQUIRE(reader.ReadInt() == -6);

    view_reader = MakeViewReader("+foo\r\n");
    REQUIRE(reader.ReadType() == redis::ERespType::SimpleString);
    REQUIRE(reader.ReadSimpleString() == "foo");
}

TEST_CASE_METHOD(ReaderTest, "LoopReader") {
    auto loop_reader = MakeLoopReader("$6\r\nfoobar\r\n");
    redis::RespReader reader{&loop_reader};
    for (auto i = 0; i < 10; ++i) {
        REQUIRE(reader.ReadType() == redis::ERespType::BulkString);
        REQUIRE(reader.ReadBulkString() == "foobar");
    }
}

TEST_CASE_METHOD(ReaderTest, "MinMaxInt64") {
    constexpr auto kMin = std::numeric_limits<int64_t>::min();
    constexpr auto kMax = std::numeric_limits<int64_t>::max();

    auto data = std::to_string(kMin) + "\r\n" + std::to_string(kMax) + "\r\n";
    auto reader = MakeReader(data.c_str());
    CHECK(reader.ReadInt() == kMin);
    CHECK(reader.ReadInt() == kMax);
}

TEST_CASE_METHOD(ReaderTest, "LimitIntReading") {
    auto loop_reader = MakeLoopReader("123456");
    redis::RespReader reader{&loop_reader};
    CHECK_THROWS_AS(reader.ReadInt(), redis::RedisError);
}

TEST_CASE_METHOD(ReaderTest, "Exceptions") {
    CheckThrowsAll("");
    CheckThrowsAll("&");
    CheckThrowsAll("abacaba");

    CheckThrows<kInt, kType>("9223372036854775808\r\n");
    CheckThrows<kInt>("-9223372036854775809\r\n");
    CheckThrows<kInt, kType>("4aba\r\n");
    CheckThrows<kInt>("-42caba\r\n");
    CheckThrows<kInt, kType>("15 \r\n");
    CheckThrows<kInt, kType>(" -22\r\n");

    CheckThrows<kArray, kBulk>("-2\r\n");
    CheckThrows<kArray, kBulk>("-33\r\n");

    CheckThrows<kString, kError>("aba\rcaba\r\n");
    CheckThrows<kString, kError>("aba\r\r");
    CheckThrows<kString, kError>("aba\ncaba\r\n");
    CheckThrows<kString, kError>("aba\n\n");
}
