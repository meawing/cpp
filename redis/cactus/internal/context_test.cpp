#include <catch2/catch_test_macros.hpp>

#include <cactus/internal/context.h>

#include <iostream>
#include <functional>

using namespace cactus;

struct FnContextEntry : public IContextEntry {
    template <class Fn>
    FnContextEntry(Fn fn) : fn(fn) {
    }

    std::function<SavedContext*()> fn;

    virtual SavedContext* Run() override {
        return fn();
    }
};

TEST_CASE("Simple context switch") {
    std::vector<char> stack(512 * 1024);
    SavedContext fiber_context{stack.data(), stack.size()};
    SavedContext main_context;

    bool activated = false;
    bool activated_twice = false;
    FnContextEntry switch_back = [&]() -> SavedContext* {
        activated = true;
        main_context.Activate(&fiber_context);
        activated_twice = true;
        return &main_context;
    };
    fiber_context.entry = &switch_back;

    fiber_context.Activate(&main_context);
    REQUIRE(activated == true);
    REQUIRE(activated_twice != true);

    fiber_context.Activate(&main_context);
    REQUIRE(activated == true);
    REQUIRE(activated_twice == true);
}

TEST_CASE("Exception context saved and restored on switch") {
    std::vector<char> stack(512 * 1024);
    SavedContext fiber_context{stack.data(), stack.size()};
    SavedContext main_context;

    FnContextEntry throw_exceptions = [&]() -> SavedContext* {
        try {
            throw std::exception();
        } catch (std::exception& ex) {
            auto my_exception = std::current_exception();
            REQUIRE(std::uncaught_exceptions() == 0);

            main_context.Activate(&fiber_context);

            REQUIRE(my_exception == std::current_exception());
            REQUIRE(std::uncaught_exceptions() == 0);
        }

        return &main_context;
    };
    fiber_context.entry = &throw_exceptions;

    fiber_context.Activate(&main_context);
    try {
        throw std::exception();
    } catch (std::exception& ex) {
        auto my_exception = std::current_exception();
        REQUIRE(std::uncaught_exceptions() == 0);

        fiber_context.Activate(&main_context);

        REQUIRE(my_exception == std::current_exception());
        REQUIRE(std::uncaught_exceptions() == 0);
    }
}
