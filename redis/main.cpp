#include "redis-server/server.h"
#include "redis-server/simple_storage.h"

#include <cactus/cactus.h>

int main() {
    cactus::Scheduler{}.Run([] {
        auto storage = std::make_unique<redis::SimpleStorage>();
        redis::Server server{{"0.0.0.0", 6'379}, std::move(storage)};
        server.Run();
    });
}
