#pragma once

#include <string_view>
#include <string>

#include <cactus/io/writer.h>

namespace redis {

class RespWriter {
public:
    explicit RespWriter(cactus::IWriter* writer);

    void WriteSimpleString(std::string_view s);
    void WriteError(std::string_view s);
    void WriteInt(int64_t n);

    void WriteBulkString(std::string_view s);
    void WriteNullBulkString();

    void StartArray(size_t size);
    void WriteNullArray();

    // void WriteArrayInts(auto&& range);

    void WriteArrayInts(auto&& range) {
        size_t len = 0;
        for (const auto& elem : range) {
            len = len + 1 + elem - elem;
        }
        std::string res = "*" + std::to_string(len) + "\r\n";
        writer_->Write(cactus::View(res));
        for (const auto& elem : range) {
            std::string res = ":" + std::to_string(elem) + "\r\n";
            writer_->Write(cactus::View(res));
        }
    }

private:
    cactus::IWriter* writer_;
};

}  // namespace redis
