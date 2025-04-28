#pragma once

#include "../resp_types.h"

#include <optional>
#include <string_view>

#include <cactus/io/reader.h>
#include <vector>

namespace redis {

class RespReader {
public:
    explicit RespReader(cactus::IReader* reader);

    ERespType ReadType();

    std::string_view ReadSimpleString();
    std::string_view ReadError();
    int64_t ReadInt();
    std::optional<std::string_view> ReadBulkString();
    int64_t ReadArrayLength();

private:
    static constexpr size_t kMaxLineLength = 64;  // Maximum allowed line length for integers
    std::string line_buffer_;  // Using string instead of vector for better small buffer optimization
    std::vector<char> bulk_string_buffer_;
    cactus::IReader* reader_;

    std::string_view ReadLine();
};

}  // namespace redis
