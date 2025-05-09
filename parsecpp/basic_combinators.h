// File: basic_combinators.h
#pragma once

#include "types.h"
#include "integral_constant.h"

#include <deque>
#include <optional>
#include <string>
#include <expected>
#include <type_traits>
#include <utility>

namespace ps {

// ----------------------------------------------------------------------------
// PSkip
// ----------------------------------------------------------------------------
template <CParser Parser>
struct PSkip {
    PSkip() = default;
    explicit constexpr PSkip(Parser) {
    }

    static constexpr auto operator()(Input in) noexcept {
        auto res = Parser{}(in);
        if (!res) {
            // Propagate the error returned by the inner parser.
            return ParseResult<Unit>{std::unexpected(res.error())};
        }
        return ParseResult<Unit>{Output<Unit>{Unit{}, res->tail}};
    }
};

// ----------------------------------------------------------------------------
// PMany
// ----------------------------------------------------------------------------
template <CParser Parser, size_t Min, size_t Max>
struct PMany {
    static_assert(Min <= Max, "Min must be <= Max");

    PMany() = default;
    constexpr PMany(Parser, SSizet<Min>, SSizet<Max>) {
    }

    static constexpr auto operator()(Input in) noexcept {
        // Deduce the element type from the inner parser.
        using ElementType = decltype(Parser{}(in)->val);
        std::deque<ElementType> elems;
        Input current = in;
        size_t count = 0;

        // Try to parse repeatedly until we reach Max elements or get an error.
        while (count < Max) {
            auto res = Parser{}(current);
            if (!res) {
                break;
            }
            elems.push_back(res->val);
            current = res->tail;
            ++count;
        }
        if (elems.size() < Min) {
            return ParseResult<std::deque<ElementType>>{
                std::unexpected(ParseError{"PMany: not enough tokens", current})};
        }
        return ParseResult<std::deque<ElementType>>{
            Output<std::deque<ElementType>>{elems, current}};
    }
};

// ----------------------------------------------------------------------------
// PMaybe
// ----------------------------------------------------------------------------
template <CParser Parser>
struct PMaybe {
    PMaybe() = default;
    explicit constexpr PMaybe(Parser) {
    }

    static constexpr auto operator()(Input in) noexcept {
        using InnerType = decltype(Parser{}(in)->val);
        auto res = Parser{}(in);
        if (res) {
            return ParseResult<std::optional<InnerType>>{
                Output<std::optional<InnerType>>{std::optional<InnerType>{res->val}, res->tail}};
        } else {
            return ParseResult<std::optional<InnerType>>{
                Output<std::optional<InnerType>>{std::nullopt, in}};
        }
    }
};

// ----------------------------------------------------------------------------
// PApply
// ----------------------------------------------------------------------------
template <typename Applier, CParser ParserT>
struct PApply {
    PApply() = default;
    constexpr PApply(Applier, ParserT) {
    }

    static constexpr auto operator()(Input in) noexcept {
        auto res = ParserT{}(in);
        if (!res) {
            // Compute the result type R by applying Applier to the inner parser's value.
            // We use std::decay_t to strip any reference qualifiers.
            using R = std::decay_t<decltype(Applier{}(res->val).value())>;
            return ParseResult<R>{std::unexpected(res.error())};
        }
        auto applied = Applier{}(res->val);
        using R = std::decay_t<decltype(applied.value())>;
        if (applied) {
            return ParseResult<R>{Output<R>{std::move(applied.value()), res->tail}};
        } else {
            return ParseResult<R>{std::unexpected(ParseError{applied.error(), res->tail})};
        }
    }
};

}  // namespace ps
