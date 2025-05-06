#pragma once

#include <string_view>

namespace ps {
template <char... s>
struct SString {
    static constexpr std::string_view operator()() noexcept;

    static constexpr size_t Size() noexcept;
};

/*
namespace literals {
__extension__ constexpr ??? operator""_cs() noexcept { 
}
*/

//??? operator==(...);

//??? operator!=(...);

//??? operator+(...);

}
}
