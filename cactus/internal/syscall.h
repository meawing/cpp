#pragma once

#include <errno.h>

namespace cactus {

template <class Syscall, class... Args>
inline int RestartEIntr(Syscall syscall, Args... args) {
restart:
    int ret = syscall(args...);
    if (ret == -1 && errno == EINTR) {
        goto restart;
    }
    return ret;
}

}  // namespace cactus
