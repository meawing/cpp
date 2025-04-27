#include <cactus/cactus.h>

void TerminateHandler() {
    _exit(0);
}

int main() {
    std::set_terminate(TerminateHandler);

    cactus::Scheduler sched;
    sched.Run([&] {
        cactus::Fiber fiber([] { throw std::runtime_error("foo"); });

        cactus::Yield();
    });

    return 1;
}
