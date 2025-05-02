#include "lib.h"
#include "fun.h"

int my_struct::foo() {
    int MY_VAR = 1;
    MY_VAR = 2;
    return MY_VAR;
}

int Var = 3;

template <class T>
struct templateStruct;

template <class T>
template <class U>
void templateStruct<T>::foo_bar(U&& param) {
}

int main() {
}
