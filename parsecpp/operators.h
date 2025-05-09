#pragma once

#include "advanced_combinators.h"

namespace ps {
namespace operators {
template <CParser P1, CParser P2>
constexpr auto operator|(P1 p1, P2 p2) {
    return PChoice{p1, p2};
}

template <CParser P, CParser... Ps>
constexpr auto operator|(PChoice<Ps...> choice, P p) {
    (void)choice;
    return PChoice{Ps{}..., p};
}

template <CParser P1, CParser P2>
constexpr auto operator+(P1 p1, P2 p2) {
    return PSeq{p1, p2};
}

template <CParser P, CParser... Ps>
constexpr auto operator+(PSeq<Ps...> seq, P p) {
    (void)seq;
    return PSeq{Ps{}..., p};
}
}  // namespace operators
}  // namespace ps
