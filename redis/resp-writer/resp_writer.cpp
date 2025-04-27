#include "resp_writer.h"

namespace redis {

RespWriter::RespWriter(cactus::IWriter* writer) : writer_(writer) {
}

void RespWriter::WriteSimpleString(std::string_view s) {
    std::string res = std::string(s).c_str();
    res = "+" + res + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::WriteError(std::string_view s) {
    std::string res = std::string(s).c_str();
    res = "-" + res + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::WriteInt(int64_t n) {
    std::string res = std::to_string(n);
    res = ":" + res + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::WriteBulkString(std::string_view s) {
    size_t len_s = s.size();
    std::string res = "$" + std::to_string(len_s);
    res = res + "\r\n";
    res = res + std::string(s).c_str() + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::WriteNullBulkString() {
    std::string res = "$-1";
    res = res + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::StartArray(size_t size) {
    std::string res = "*" + std::to_string(size);
    res = res + "\r\n";
    writer_->Write(cactus::View(res));
}

void RespWriter::WriteNullArray() {
    std::string res = "*-1";
    res = res + "\r\n";
    writer_->Write(cactus::View(res));
}

// void RespWriter::WriteArrayInts(auto&& range) {
//     size_t len = 0;
//     for (const auto& elem : range) {
//         len = len + 1 + elem - elem;
//     }
//     std::string res = "*" + std::to_string(len) + "\r\n";
//     writer_->Write(cactus::View(res));
//     for (const auto& elem : range) {
//         std::string res = ":" + std::to_string(elem) + "\r\n";
//         writer_->Write(cactus::View(res));
//     }
// }

}  // namespace redis