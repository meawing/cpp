#include "pow.h"
#include "sqrt.h"

template <bool>
struct StaticAssert {};

template <>
struct StaticAssert<true> {
    static const int value = 0;
};

#define STATIC_ASSERT(E) (void)StaticAssert<E>::value;

int main() {
    STATIC_ASSERT((Pow<2, 2>::kValue == 4));
    STATIC_ASSERT((Pow<3, 4>::kValue == 81));
    STATIC_ASSERT((Pow<1, 10>::kValue == 1));
    STATIC_ASSERT((Pow<4, 1>::kValue == 4));
    STATIC_ASSERT((Pow<7, 0>::kValue == 1));
    STATIC_ASSERT((Pow<2, 10>::kValue == 1024));

    STATIC_ASSERT((Sqrt<9>::kValue == 3));
    STATIC_ASSERT((Sqrt<1>::kValue == 1));
    STATIC_ASSERT((Sqrt<2>::kValue == 2));
    STATIC_ASSERT((Sqrt<10>::kValue == 4));
    STATIC_ASSERT((Sqrt<143>::kValue == 12));
    STATIC_ASSERT((Sqrt<100000000>::kValue == 10000));
    STATIC_ASSERT((Sqrt<1000000007>::kValue == 31623));
}
