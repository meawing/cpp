#pragma once

#include "../resp_types.h"

#include <optional>
#include <string_view>

#include <cactus/io/reader.h>

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
};

}  // namespace redis
