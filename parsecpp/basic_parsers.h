#pragma once

#include "types.h"
#include "string_constant.h"

// In basic_parsers.h (adding implementations)

namespace ps {

struct PEof {
    static constexpr ParseResult<Unit> operator()(Input in) noexcept {
        if (in.empty()) {
            return Output<Unit>{Unit{}, in};
        }
        return std::unexpected(ParseError{"Expected EOF", in});
    }
};

struct PAnyChar {
    static constexpr ParseResult<char> operator()(Input in) noexcept {
        if (in.empty()) {
            return std::unexpected(ParseError{"EOF", in});
        }
        return Output<char>{in[0], in.substr(1)};
    }
};

template <char c>
struct PChar {
    static constexpr ParseResult<char> operator()(Input in) noexcept {
        if (in.empty()) {
            return std::unexpected(ParseError{"EOF", in});
        }
        if (in[0] != c) {
            return std::unexpected(
                ParseError{std::string("Unexpected symbol '") + in[0] + "'", in});
        }
        return Output<char>{c, in.substr(1)};
    }
};

template <char... chars>
struct PString {
    PString() = default;
    explicit constexpr PString(SString<chars...>) {};

    static constexpr ParseResult<std::string_view> operator()(Input in) noexcept {
        constexpr auto kExpected = SString<chars...>{}();

        if (in.size() < sizeof...(chars)) {
            return std::unexpected(ParseError{"Input too short for PString", in});
        }

        std::string_view actual = in.substr(0, sizeof...(chars));
        if (actual != kExpected) {
            return std::unexpected(ParseError{"Strings do not match", in});
        }

        return Output<std::string_view>{actual, in.substr(sizeof...(chars))};
    }
};

template <char... chars>
struct PSomeChar {
    static_assert(sizeof...(chars) > 0);

    static constexpr ParseResult<char> operator()(Input in) noexcept {
        if (in.empty()) {
            return std::unexpected(ParseError{"EOF", in});
        }

        const char c = in[0];
        if (((c == chars) || ...)) {
            return Output<char>{c, in.substr(1)};
        }

        std::string error_msg = "Expected one of: ";
        ((error_msg += std::string("'") + chars + "', "), ...);
        error_msg += "got '" + std::string(1, c) + "'";

        return std::unexpected(ParseError{error_msg, in});
    }
};

}  // namespace ps
