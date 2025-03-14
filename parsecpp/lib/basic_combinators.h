#pragma once

#include "types.h"
#include "integral_constant.h"

namespace ps {
template <CParser Parser>
struct PSkip {
    PSkip() = default;

    explicit constexpr PSkip(Parser) {}
    static constexpr auto operator()(Input in) noexcept;
};

template <CParser Parser, size_t Min, size_t Max>
struct PMany {
    static_assert(Min <= Max);

    PMany() = default;

    constexpr PMany(Parser, SSizet<Min>, SSizet<Max>) { }

    static constexpr auto operator()(Input in) noexcept;
};

template <CParser Parser>
struct PMaybe {

    PMaybe() = default;

    explicit constexpr PMaybe(Parser) {}

    static constexpr auto operator()(Input in) noexcept;
};


template <typename Applier, CParser ParserT>
struct PApply {
    PApply() = default;

    constexpr PApply(Applier, ParserT) { };

    static constexpr auto operator()(Input in) noexcept;
};

}
