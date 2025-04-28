#include "resp_reader.h"

#include <charconv>
#include <stdexcept>

namespace redis {

RespReader::RespReader(cactus::IReader* reader) : reader_(reader) {}

ERespType RespReader::ReadType() {
    char type_char;
    if (reader_->Read(cactus::View(&type_char, 1)) != 1) {
        throw RedisError("Unexpected end of input");
    }

    switch (type_char) {
        case '+': return ERespType::SimpleString;
        case '-': return ERespType::Error;
        case ':': return ERespType::Int;
        case '$': return ERespType::BulkString;
        case '*': return ERespType::Array;
        default: throw RedisError("Invalid RESP type");
    }
}

std::string_view RespReader::ReadSimpleString() {
    return ReadLine();
}

std::string_view RespReader::ReadError() {
    return ReadLine();
}

int64_t RespReader::ReadInt() {
    auto line = ReadLine();
    
    // Strict validation
    if (line.empty()) {
        throw RedisError("Empty integer value");
    }

    // Check for valid characters
    size_t start = 0;
    if (line[0] == '-') {
        start = 1;
        if (line.size() == 1) {
            throw RedisError("Invalid integer format");
        }
    }
    
    for (size_t i = start; i < line.size(); ++i) {
        if (!isdigit(line[i])) {
            throw RedisError("Invalid integer format");
        }
    }

    // Manual bounds checking for 64-bit integers
    constexpr int64_t kMaxBeforeOverflow = INT64_MAX / 10;
    constexpr int64_t kMinBeforeUnderflow = INT64_MIN / 10;
    
    int64_t value = 0;
    bool negative = (line[0] == '-');
    size_t i = negative ? 1 : 0;
    
    for (; i < line.size(); ++i) {
        int digit = line[i] - '0';
        
        if (negative) {
            if (value < kMinBeforeUnderflow || 
               (value == kMinBeforeUnderflow && digit > 8)) {
                throw RedisError("Integer out of range");
            }
            value = value * 10 - digit;
        } else {
            if (value > kMaxBeforeOverflow || 
               (value == kMaxBeforeOverflow && digit > 7)) {
                throw RedisError("Integer out of range");
            }
            value = value * 10 + digit;
        }
    }
    
    return value;
}


std::optional<std::string_view> RespReader::ReadBulkString() {
    int64_t length = ReadInt();
    if (length == -1) {
        // Null bulk string
        return std::nullopt;
    }

    if (length < 0) {
        throw RedisError("Invalid bulk string length");
    }

    // Read the bulk string content
    bulk_string_buffer_.resize(length);
    if (length > 0) {
        size_t bytes_read = 0;
        while (bytes_read < static_cast<size_t>(length)) {
            auto chunk = cactus::View(
                bulk_string_buffer_.data() + bytes_read,
                length - bytes_read
            );
            bytes_read += reader_->Read(chunk);
        }
    }

    // Read and verify CRLF terminator
    char crlf[2];
    if (reader_->Read(cactus::View(crlf, 2)) != 2) {
        throw RedisError("Unexpected end of input");
    }
    if (crlf[0] != '\r' || crlf[1] != '\n') {
        throw RedisError("Invalid bulk string termination");
    }

    return std::string_view(bulk_string_buffer_.data(), length);
}

int64_t RespReader::ReadArrayLength() {
    return ReadInt();
}

std::string_view RespReader::ReadLine() {
    line_buffer_.clear();
    line_buffer_.reserve(kMaxLineLength);  // Prevent large allocations
    
    while (true) {
        if (line_buffer_.size() >= kMaxLineLength) {
            throw RedisError("Line too long");
        }

        char c;
        reader_->Read(cactus::View(&c, 1));
        
        if (c == '\r') {
            char next;
            reader_->Read(cactus::View(&next, 1));
            if (next != '\n') {
                throw RedisError("Invalid line termination");
            }
            break;
        }
        
        if (c == '\n') {
            throw RedisError("Invalid line termination");
        }
        
        line_buffer_.push_back(c);
    }
    
    return line_buffer_;
}


}  // namespace redis