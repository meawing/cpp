#include <cactus/test.h>

#include <cactus/cactus.h>

using namespace cactus;

FIBER_TEST_CASE("Spawn fibers in group") {
    int done = 0;

    ServerGroup g;

    g.Spawn([&] {
        SleepFor(std::chrono::milliseconds(1));
        done++;
    });

    g.Spawn([&] {
        SleepFor(std::chrono::milliseconds(1));
        done++;
    });

    SleepFor(std::chrono::milliseconds(10));
    REQUIRE(done == 2);
}

FIBER_TEST_CASE("Wait empty group") {
    WaitGroup g;
    g.Wait();
}

FIBER_TEST_CASE("Server group cancel") {
    int done = 0;

    {
        ServerGroup g;
        g.Spawn([&] {
            try {
                SleepFor(std::chrono::seconds(1));
            } catch (const FiberCanceledException&) {
                done++;
            }
        });

        g.Spawn([&] {
            SleepFor(std::chrono::milliseconds(1));
            done++;
        });

        SleepFor(std::chrono::milliseconds(10));
    }

    REQUIRE(done == 2);
}

FIBER_TEST_CASE("Server group ignores errors") {
    ServerGroup g;
    g.Spawn([&] { throw std::runtime_error("unhandled error"); });

    SleepFor(std::chrono::milliseconds(10));
}

FIBER_TEST_CASE("Wait group returns first error") {
    WaitGroup g;
    g.Spawn([&] { SleepFor(std::chrono::seconds(1)); });
    g.Spawn([&] { throw std::logic_error("error"); });
    g.Spawn([&] { throw std::runtime_error("unhandled error"); });

    REQUIRE_THROWS_AS(g.Wait(), std::logic_error);
}
