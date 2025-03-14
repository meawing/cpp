#include "next_prime.h"
#include "determinant.h"

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, "")

int main() {
    STATIC_ASSERT(NextPrime(1) == 2);
    STATIC_ASSERT(NextPrime(2) == 2);
    STATIC_ASSERT(NextPrime(9) == 11);
    STATIC_ASSERT(NextPrime(17'230) == 17'231);
    STATIC_ASSERT(NextPrime(121) == 127);
    STATIC_ASSERT(NextPrime(1'000'000) == 1'000'003);
    STATIC_ASSERT(NextPrime(1'000'000'000) == 1'000'000'007);

    STATIC_ASSERT(Determinant<2>({{{2, 3}, {1, 4}}}) == 5);
    STATIC_ASSERT(Determinant<3>({{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}}) == 0);
    STATIC_ASSERT(Determinant<1>({{{3}}}) == 3);
    STATIC_ASSERT(Determinant<5>({{{3, -5, 1, -4, 2},
                                   {-5, -4, -2, -3, -2},
                                   {5, 3, 0, -3, -3},
                                   {2, -2, 0, 2, -1},
                                   {-1, -1, 5, -3, -5}}}) == -7'893);
}
