#include "poller.h"

#include "cactus/core/debug.h"
#include "cactus/core/scheduler.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <system_error>

#include <glog/logging.h>

namespace cactus {

Poller::Poller() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::system_error(errno, std::system_category(), "epoll_create1");
    }
}

Poller::~Poller() {
    int ret = close(epoll_fd_);
    PCHECK(ret == 0) << "close() system call failed";
    epoll_fd_ = -1;

    while (free_list_) {
        auto desc = free_list_;
        free_list_ = free_list_->next;
        delete desc;
    }
}

void Poller::Poll(Duration* timeout) {
    constexpr int kEpollBatchSize = 64;
    epoll_event events[kEpollBatchSize];

    int epoll_timeout = 0;
    if (timeout != nullptr) {
        if (*timeout == Duration::max()) {
            epoll_timeout = -1;
        } else {
            epoll_timeout = std::max<int>(
                5, std::chrono::duration_cast<std::chrono::milliseconds>(*timeout).count());
        }
    }

    int nevents = epoll_wait(epoll_fd_, events, kEpollBatchSize, epoll_timeout);
    if (nevents == -1) {
        if (errno == EINTR) {
            return;
        }

        throw std::system_error(errno, std::system_category(), "epoll_wait");
    }

    for (int i = 0; i < nevents; ++i) {
        FDDesc* desc = static_cast<FDDesc*>(events[i].data.ptr);
        if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
            if (desc->reader) {
                desc->reader->Unpark();
                desc->reader = nullptr;
            }
        }

        if (events[i].events & (EPOLLOUT | EPOLLHUP | EPOLLERR)) {
            if (desc->writer) {
                desc->writer->Unpark();
                desc->writer = nullptr;
            }
        }
    }
}

FDDesc* Poller::NewDesc() {
    if (free_list_) {
        auto desc = free_list_;
        free_list_ = desc->next;
        return desc;
    }

    return new FDDesc{};
}

void Poller::FreeDesc(FDDesc* desc) {
    desc->next = free_list_;
    free_list_ = desc;
}

FDWatcher::FDWatcher(Poller* poller) : poller_(poller) {
}

FDWatcher::~FDWatcher() {
    DCHECK(!desc_);
}

void FDWatcher::Init(int fd) {
    desc_ = poller_->NewDesc();
    epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    event.data.ptr = desc_;

    desc_->fd = fd;

    int ret = epoll_ctl(poller_->epoll_fd_, EPOLL_CTL_ADD, fd, &event);
    if (ret != 0) {
        desc_->fd = -1;
        poller_->FreeDesc(desc_);
        throw std::system_error(errno, std::system_category(), "epoll_ctl");
    }
}

void FDWatcher::Reset() {
    int ret = epoll_ctl(poller_->epoll_fd_, EPOLL_CTL_DEL, desc_->fd, nullptr);
    PCHECK(ret == 0) << "epoll_ctl() failed";

    desc_->fd = -1;
    if (desc_->writer) {
        desc_->writer->Unpark();
        desc_->writer = nullptr;
    }
    if (desc_->reader) {
        desc_->reader->Unpark();
        desc_->reader = nullptr;
    }

    poller_->FreeDesc(desc_);
    desc_ = nullptr;
}

bool FDWatcher::Wait(bool read) {
    auto& slot = (read ? desc_->reader : desc_->writer);
    CHECK(!slot);

    slot = this_fiber;
    Park(read ? "read" : "write", [&, this] {
        if (desc_) {
            slot = nullptr;
        }
    });

    return !desc_;
}

}  // namespace cactus
