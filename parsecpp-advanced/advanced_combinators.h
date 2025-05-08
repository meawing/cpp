#pragma once

#include "types.h"
#include "integral_constant.h"

#include <variant>
#include <tuple>

namespace ps {
template <CParser... ParsersT>
struct PChoice {
    PChoice() = default;
    static_assert(sizeof...(ParsersT) > 0);

    explicit constexpr PChoice(ParsersT...) { }

    static constexpr auto operator()(Input in) noexcept;
};

template <CParser... ParsersT>
struct PSeq {
    static_assert(sizeof...(ParsersT) > 0);
    PSeq() = default;

    explicit constexpr PSeq(ParsersT...) { }

    static constexpr auto operator()(Input in) noexcept;
};
}

namespace ps {
namespace operators {
auto operator|(...);

auto operator+(...);
}
}
