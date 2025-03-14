#pragma once

#include "types.h"
#include "string_constant.h"

namespace ps {
struct PEof {
    static constexpr ParseResult<Unit> operator()(Input in) noexcept;
};

struct PAnyChar {
    static constexpr ParseResult<char> operator()(Input in) noexcept;
};

template <char c>
struct PChar {
    static constexpr ParseResult<char> operator()(Input in) noexcept;
};

template <char... chars>
struct PString {
    PString() = default;
    explicit constexpr PString(Sstring<chars...>) {};

    static constexpr ParseResult<std::string_view> operator()(Input in) noexcept;
};

template <char... chars>
struct PSomeChar {
    static_assert(sizeof...(chars) > 0);
    static constexpr ParseResult<char> operator()(Input in) noexcept;
};

}
