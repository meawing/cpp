#include "hash.h"
#include "another_pow.h"

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, "")

int main() {
    STATIC_ASSERT(Pow(2, 2) == 4);
    STATIC_ASSERT(Pow(3, 4) == 81);
    STATIC_ASSERT(Pow(1, 10) == 1);
    STATIC_ASSERT(Pow(4, 1) == 4);
    STATIC_ASSERT(Pow(7, 0) == 1);
    STATIC_ASSERT(Pow(2, 10) == 1024);

    STATIC_ASSERT(Hash("abacaba", 13, 17239) == 7755);
    STATIC_ASSERT(Hash("", 19, 21) == 0);
    STATIC_ASSERT(Hash("Hello, world!", 257, 1000000007) == 977585366);
}
