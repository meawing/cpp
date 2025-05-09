#pragma once

#include <string_view>
#include <string>
#include <concepts>
#include <expected>

namespace ps {

struct Unit {};

struct ParseError {
    std::string error_message;
    std::string_view rest;
};

using Input = std::string_view;

template <typename T>
struct Output {
    T val;
    Input tail;
};

template <typename T>
using ParseResult = std::expected<Output<T>, ParseError>;

template <typename TParser>
concept CParser = std::invocable<TParser, Input>;
}  // namespace ps
