#pragma once

#define MY_VAR X

extern int Var;

struct my_struct {
    int A;
    double b_;
    int foo();
};

template <class T>
struct templateStruct {
    template <class U>
    void foo_bar(U&& Param);
};
