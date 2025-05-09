#pragma once

#include "types.h"
#include "integral_constant.h"
#include "basic_combinators.h"
#include "basic_parsers.h"

#include <variant>
#include <tuple>
#include <type_traits>

namespace ps {

// Helper to remove duplicates from variant types
namespace detail {
// Helper to check if a type is in a list
template <typename T, typename... Ts>
struct Contains : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <typename T>
struct Contains<T> : std::false_type {};

// Helper to merge variant types without duplicates
template <typename... Types>
struct unique_variant;

template <typename... Unique, typename T, typename... Rest>
struct unique_variant<std::variant<Unique...>, T, Rest...> {
    using Type =
        std::conditional_t<Contains<T, Unique...>::value,
                           typename unique_variant<std::variant<Unique...>, Rest...>::Type,
                           typename unique_variant<std::variant<Unique..., T>, Rest...>::Type>;
};

template <typename... Unique>
struct unique_variant<std::variant<Unique...>> {
    using Type = std::variant<Unique...>;
};

// Initial case
template <typename T, typename... Rest>
struct unique_variant<T, Rest...> {
    using Type = typename unique_variant<std::variant<T>, Rest...>::Type;
};

// Helper to check if a type is Unit
template <typename T>
struct IsUnit : std::is_same<T, Unit> {};

// Helper to filter out Unit types from tuple
template <typename... Ts>
struct filter_unit_types;

template <>
struct filter_unit_types<> {
    using Type = std::tuple<>;
};

template <typename T, typename... Ts>
struct filter_unit_types<T, Ts...> {
    using Type = std::conditional_t<IsUnit<T>::value, typename filter_unit_types<Ts...>::Type,
                                    decltype(std::tuple_cat(
                                        std::declval<std::tuple<T>>(),
                                        std::declval<typename filter_unit_types<Ts...>::Type>()))>;
};

// Special case for all Unit types
template <typename... Ts>
struct AllUnits : std::bool_constant<(IsUnit<Ts>::value && ...)> {};

template <typename... Ts>
struct FilterUnitTypesWithFallback {
    using Filtered = typename filter_unit_types<Ts...>::Type;
    using Type = std::conditional_t<
        std::is_same_v<Filtered, std::tuple<>>,
        std::conditional_t<AllUnits<Ts...>::value, std::tuple<Unit>, std::tuple<>>, Filtered>;
};
}  // namespace detail

template <CParser... ParsersT>
struct PChoice {
    PChoice() = default;
    static_assert(sizeof...(ParsersT) > 0);

    explicit constexpr PChoice(ParsersT...) {
    }

    static constexpr auto operator()(Input in) noexcept {
        using ResultType = typename detail::unique_variant<decltype(ParsersT{}(in)->val)...>::Type;
        return TryParsers<ResultType, ParsersT...>(in);
    }

private:
    template <typename ResultT>
    static constexpr auto TryParsers(Input in) noexcept {
        // Base case: no more parsers to try
        return ParseResult<ResultT>{std::unexpected(ParseError{"All parsers failed", in})};
    }

    template <typename ResultT, typename First, typename... Rest>
    static constexpr auto TryParsers(Input in) noexcept {
        auto result = First{}(in);
        if (result) {
            ResultT variant_result(std::in_place_type<decltype(result->val)>, result->val);
            return ParseResult<ResultT>{Output<ResultT>{variant_result, result->tail}};
        } else if constexpr (sizeof...(Rest) > 0) {
            return TryParsers<ResultT, Rest...>(in);
        } else {
            return ParseResult<ResultT>{std::unexpected(result.error())};
        }
    }
};

template <CParser... ParsersT>
struct PSeq {
    static_assert(sizeof...(ParsersT) > 0);
    PSeq() = default;

    explicit constexpr PSeq(ParsersT...) {
    }

    static constexpr auto operator()(Input in) noexcept {
        // Get the result type after filtering out Unit types
        using ResultType =
            typename detail::FilterUnitTypesWithFallback<decltype(ParsersT{}(in)->val)...>::Type;

        return ParseSequence<ResultType, ParsersT...>(in, std::tuple<>{});
    }

private:
    // Base case: no more parsers
    template <typename ResultT>
    static constexpr auto ParseSequence(Input in, [[maybe_unused]] std::tuple<>) noexcept {
        return ParseResult<ResultT>{Output<ResultT>{ResultT{}, in}};
    }

    // Recursive case: parse next parser
    template <typename ResultT, typename First, typename... Rest>
    static constexpr auto ParseSequence(Input in, auto accum) noexcept {
        auto result = First{}(in);
        if (!result) {
            return ParseResult<ResultT>{std::unexpected(result.error())};
        }

        if constexpr (std::is_same_v<decltype(result->val), Unit>) {
            // Skip Unit values
            if constexpr (sizeof...(Rest) > 0) {
                return ParseSequence<ResultT, Rest...>(result->tail, accum);
            } else {
                // If everything was Unit and we have an empty accumulator
                if constexpr (std::is_same_v<decltype(accum), std::tuple<>> &&
                              std::is_same_v<ResultT, std::tuple<Unit>>) {
                    return ParseResult<ResultT>{Output<ResultT>{ResultT{Unit{}}, result->tail}};
                } else {
                    return ParseResult<ResultT>{Output<ResultT>{ResultT(accum), result->tail}};
                }
            }
        } else {
            // Add non-Unit value to accumulator
            auto new_accum =
                std::tuple_cat(std::move(accum), std::tuple<decltype(result->val)>{result->val});
            if constexpr (sizeof...(Rest) > 0) {
                return ParseSequence<ResultT, Rest...>(result->tail, new_accum);
            } else {
                return ParseResult<ResultT>{Output<ResultT>{ResultT(new_accum), result->tail}};
            }
        }
    }
};
}  // namespace ps
