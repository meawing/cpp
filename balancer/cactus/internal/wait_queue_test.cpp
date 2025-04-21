#include <cactus/test.h>

#include <cactus/cactus.h>

using namespace cactus;

FIBER_TEST_CASE("Wait queue cancelation") {
    WaitQueue wq;

    ServerGroup g;
    bool woken_up = false;

    {
        ServerGroup canceled_g;
        canceled_g.Spawn([&] {
            auto finally = cactus::MakeGuard([&] { wq.NotifyOne(); });

            wq.Wait();
        });

        canceled_g.Spawn([&] { wq.Wait(); });

        g.Spawn([&] {
            wq.Wait();
            woken_up = true;
        });

        SleepFor(std::chrono::milliseconds(1));
    }

    SleepFor(std::chrono::milliseconds(1));

    REQUIRE(woken_up);
}
