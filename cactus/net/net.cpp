#include "net.h"

#include "cactus/net/address.h"
#include "cactus/core/scheduler.h"
#include "poller.h"
#include "cactus/internal/syscall.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <glog/logging.h>

namespace cactus {

class SocketConn final : public IConn {
public:
    SocketConn(const cactus::SocketAddress& remote, int fd)
        : fd_(fd), watcher_(&this_scheduler->poller), remote_(remote) {
        try {
            watcher_.Init(fd_);
        } catch (...) {
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            fd_ = -1;
            throw;
        }
    }

    explicit SocketConn(const cactus::SocketAddress& remote)
        : watcher_(&this_scheduler->poller), remote_(remote) {
        fd_ = socket(remote.GetFamily(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

        if (fd_ == -1) {
            throw std::system_error(errno, std::system_category(), "socket");
        }

        try {
            watcher_.Init(fd_);
        } catch (...) {
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            throw;
        }
    }

    ~SocketConn() final {
        Close();
    }

    virtual size_t Read(MutableView buf) override {
        while (true) {
            int ret = RestartEIntr(read, fd_, buf.data(), buf.size());
            if (ret == -1 && errno == EAGAIN) {
                bool closed = watcher_.Wait(true);
                if (closed) {
                    throw std::runtime_error("socket was closed");
                }
                continue;
            }

            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "read");
            }

            return ret;
        }
    }

    virtual void Write(ConstView buf) override {
        while (buf.size() != 0) {
            int ret = RestartEIntr(send, fd_, buf.data(), buf.size(), MSG_NOSIGNAL);
            if (ret == -1 && errno == EAGAIN) {
                bool closed = watcher_.Wait(false);
                if (closed) {
                    throw std::runtime_error("socket was closed");
                }
                continue;
            }

            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "read");
            }

            buf = buf.subspan(ret);
        }
    }

    virtual void CloseWrite() override {
        int ret = RestartEIntr(shutdown, fd_, SHUT_WR);
        if (ret == -1) {
            throw std::system_error(errno, std::system_category(), "shutdown");
        }
    }

    virtual void Close() override {
        if (fd_ != -1) {
            watcher_.Reset();
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            fd_ = -1;
        }
    }

    const cactus::SocketAddress& RemoteAddress() const override {
        return remote_;
    }

    const cactus::SocketAddress& LocalAddress() const override {
        return local_;
    }

    void SetLocalAddress(const cactus::SocketAddress& local) {
        local_ = local;
    }

    void FinishDial() {
        sockaddr_storage ss;
        socklen_t slen = remote_.GetAddress(&ss);

        while (true) {
            int ret = RestartEIntr(connect, fd_, reinterpret_cast<const sockaddr*>(&ss), slen);
            if (ret == -1 && errno != EINPROGRESS) {
                throw std::system_error(errno, std::system_category(), "connect");
            }

            while (true) {
                bool closed = watcher_.Wait(false);
                CHECK(!closed);

                int error;
                socklen_t error_size = sizeof(error);
                ret = RestartEIntr(getsockopt, fd_, SOL_SOCKET, SO_ERROR, &error, &error_size);
                if (ret != 0) {
                    throw std::system_error(errno, std::system_category(), "getsockopt");
                }

                if (error == 0) {
                    int ret =
                        RestartEIntr(getsockname, fd_, reinterpret_cast<sockaddr*>(&ss), &slen);
                    if (ret == -1) {
                        throw std::system_error(errno, std::system_category(), "getsockname");
                    }

                    local_.SetFromSockaddr(reinterpret_cast<sockaddr*>(&ss), slen);
                    return;
                } else if (error != EINPROGRESS) {
                    throw std::system_error(error, std::system_category(), "async_connect");
                }
            }
        }
    }

private:
    int fd_ = -1;
    FDWatcher watcher_;

    cactus::SocketAddress remote_;
    cactus::SocketAddress local_;
};

std::unique_ptr<IConn> DialTCP(const cactus::SocketAddress& to) {
    auto conn = std::make_unique<SocketConn>(to);
    conn->FinishDial();
    return conn;
}

class SocketListener final : public IListener {
public:
    SocketListener(const cactus::SocketAddress& at) : watcher_(&this_scheduler->poller), at_(at) {
        try {
            fd_ = ::socket(at_.GetFamily(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

            if (fd_ == -1) {
                throw std::system_error(errno, std::system_category(), "socket");
            }

            int opt = 1;
            if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                throw std::system_error(errno, std::system_category(), "setsockopt(SO_REUSEADDR)");
            }

            sockaddr_storage ss;
            socklen_t slen = at_.GetAddress(&ss);

            int ret = ::bind(fd_, reinterpret_cast<const sockaddr*>(&ss), slen);
            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "bind");
            }

            ret = ::listen(fd_, 128);
            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "listen");
            }

            ret = ::getsockname(fd_, reinterpret_cast<sockaddr*>(&ss), &slen);
            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "getsockname");
            }

            at_.SetFromSockaddr(reinterpret_cast<sockaddr*>(&ss), slen);

            watcher_.Init(fd_);
        } catch (...) {
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            fd_ = -1;
            throw;
        }
    }

    virtual void Close() {
        if (fd_ != -1) {
            watcher_.Reset();
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            fd_ = -1;
        }
    }

    virtual const cactus::SocketAddress& Address() const {
        return at_;
    }

    virtual std::unique_ptr<IConn> Accept() {
        while (true) {
            sockaddr_storage ss;
            socklen_t slen = sizeof(ss);

            int fd = RestartEIntr(accept4, fd_, reinterpret_cast<sockaddr*>(&ss), &slen,
                                  SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd == -1 && errno != EAGAIN) {
                throw std::system_error(errno, std::system_category(), "accept");
            } else if (fd == -1 && errno == EAGAIN) {
                bool closed = watcher_.Wait(true);
                if (closed) {
                    throw std::runtime_error("socket was closed");
                }
                continue;
            }

            cactus::SocketAddress peer;
            peer.SetFromSockaddr(reinterpret_cast<sockaddr*>(&ss), slen);
            auto conn = std::make_unique<SocketConn>(peer, fd);
            conn->SetLocalAddress(at_);
            return conn;
        }
    }

    ~SocketListener() final {
        Close();
    }

private:
    int fd_ = -1;
    FDWatcher watcher_;

    cactus::SocketAddress at_;
};

std::unique_ptr<IListener> ListenTCP(const cactus::SocketAddress& at) {
    return std::make_unique<SocketListener>(at);
}

class PacketConn : public IPacketConn {
public:
    explicit PacketConn(const cactus::SocketAddress& at)
        : watcher_(&this_scheduler->poller), at_(at) {
        fd_ = socket(at.GetFamily(), SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

        if (fd_ == -1) {
            throw std::system_error(errno, std::system_category(), "socket");
        }

        try {
            if (at_.IsInitialized()) {
                sockaddr_storage ss;
                socklen_t slen = at_.GetAddress(&ss);

                int ret = ::bind(fd_, reinterpret_cast<const sockaddr*>(&ss), slen);
                if (ret == -1) {
                    throw std::system_error(errno, std::system_category(), "bind");
                }
            }

            watcher_.Init(fd_);
        } catch (...) {
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            throw;
        }
    }

    ~PacketConn() {
        Close();
    }

    size_t RecvFrom(MutableView buf, cactus::SocketAddress* from) override {
        while (true) {
            sockaddr_storage ss;
            socklen_t slen = sizeof(ss);

            int ret = RestartEIntr(recvfrom, fd_, buf.data(), buf.size(), 0,
                                   reinterpret_cast<sockaddr*>(&ss), &slen);
            if (ret == -1 && errno == EAGAIN) {
                bool closed = watcher_.Wait(true);
                if (closed) {
                    throw std::runtime_error("socket was closed");
                }
                continue;
            }

            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "recvfrom");
            }

            from->SetFromSockaddr(reinterpret_cast<sockaddr*>(&ss), slen);
            return ret;
        }
    }

    size_t SendTo(ConstView buf, const cactus::SocketAddress& to) override {
        while (true) {
            sockaddr_storage ss;
            socklen_t slen = to.GetAddress(&ss);

            int ret = RestartEIntr(sendto, fd_, buf.data(), buf.size(), MSG_NOSIGNAL,
                                   reinterpret_cast<sockaddr*>(&ss), slen);
            if (ret == -1 && errno == EAGAIN) {
                bool closed = watcher_.Wait(false);
                if (closed) {
                    throw std::runtime_error("socket was closed");
                }
                continue;
            }

            if (ret == -1) {
                throw std::system_error(errno, std::system_category(), "read");
            }

            return ret;
        }
    }

private:
    int fd_ = -1;
    FDWatcher watcher_;

    cactus::SocketAddress at_;

    void Close() override {
        if (fd_ != -1) {
            watcher_.Reset();
            int ret = close(fd_);
            PCHECK(ret == 0) << "close() system call failed";
            fd_ = -1;
        }
    }
};

std::unique_ptr<IPacketConn> ListenUDP(const cactus::SocketAddress& at) {
    return std::make_unique<PacketConn>(at);
}

}  // namespace cactus
