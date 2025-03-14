#pragma once

#include <ranges>
#include <iterator>
#include <algorithm>

namespace details {
template <std::ranges::forward_range R>
    requires std::ranges::view<R>
class TakeView : public std::ranges::view_interface<TakeView<R>> {
public:
    TakeView() = default;

    TakeView(R base, std::ranges::range_difference_t<R> count)
        : base_{std::move(base)}, count_{count} {
    }

    auto begin() {
        return std::ranges::begin(base_);
    }

    auto end() {
        auto end = std::ranges::end(base_);
        return std::ranges::next(begin(), count_, end);
    }

    auto size() const
        requires std::ranges::sized_range<const R>
    {
        auto n = std::ranges::size(base_);
        return std::min(n, static_cast<decltype(n)>(count_));
    }

private:
    R base_;
    std::ranges::range_difference_t<R> count_;
};

template <class R>
TakeView(R&& base, std::ranges::range_difference_t<R>) -> TakeView<std::ranges::views::all_t<R>>;

template <std::integral Difference>
struct TakeAdaptorClosure {
    Difference count;

    template <std::ranges::viewable_range R>
    auto operator()(R&& r) {
        return TakeView{std::forward<R>(r), count};
    }

    template <std::ranges::viewable_range R>
    friend auto operator|(R&& r, TakeAdaptorClosure closure) {
        return TakeView{std::forward<R>(r), closure.count};
    }
};

struct TakeAdaptor {
    template <std::ranges::viewable_range R>
    auto operator()(R&& r, std::ranges::range_difference_t<R> count) {
        return TakeView{std::forward<R>(r), count};
    }

    template <std::integral T>
    auto operator()(T count) {
        return TakeAdaptorClosure<T>{count};
    }
};
}  // namespace details

details::TakeAdaptor Take;
