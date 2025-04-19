#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <cstddef>

#ifdef __linux__
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

inline std::filesystem::path GetFileDir(std::string file) {
    std::filesystem::path path{std::move(file)};
    if (path.is_absolute()) {
        return path.parent_path();
    } else {
        throw std::runtime_error{"Bad file name"};
    }
}

class CPUTimer {
    using Clock = std::chrono::steady_clock;
    struct Times {
        Clock::duration wall_time;
        std::chrono::microseconds cpu_utime;
        std::chrono::microseconds cpu_stime;

        std::chrono::microseconds TotalCpuTime() const {
            return cpu_utime + cpu_stime;
        }
    };

public:
    enum class Type { THREAD = RUSAGE_THREAD, PROCESS = RUSAGE_SELF };

    using enum Type;

    explicit CPUTimer(Type type = Type::PROCESS) : type_{type} {
    }

    Times GetTimes() const {
        return {
            .wall_time = Clock::now() - wall_start_,
            .cpu_utime = GetCPUUTime() - cpu_ustart_,
            .cpu_stime = GetCPUSTime() - cpu_sstart_,
        };
    }

private:
#ifdef __linux__
    std::chrono::microseconds GetCPUTime(timeval rusage::* field) const {
        if (rusage usage; ::getrusage(static_cast<int>(type_), &usage)) {
            throw std::system_error{errno, std::generic_category()};
        } else {
            auto utime = usage.*field;
            return std::chrono::microseconds{1'000'000ll * utime.tv_sec + utime.tv_usec};
        }
    }
#endif

    std::chrono::microseconds GetCPUUTime() const {
#ifdef __linux__
        return GetCPUTime(&rusage::ru_utime);
#else
        auto time = Clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(time);
#endif
    }

    std::chrono::microseconds GetCPUSTime() const {
#ifdef __linux__
        return GetCPUTime(&rusage::ru_stime);
#else
        return {};
#endif
    }

    const Type type_;
    const std::chrono::time_point<Clock> wall_start_ = Clock::now();
    const std::chrono::microseconds cpu_ustart_ = GetCPUUTime();
    const std::chrono::microseconds cpu_sstart_ = GetCPUSTime();
};

#ifdef __linux__

inline int64_t GetMemoryUsage() {
    if (rusage usage; ::getrusage(RUSAGE_SELF, &usage)) {
        throw std::system_error{errno, std::generic_category()};
    } else {
        return usage.ru_maxrss;
    }
}

class MemoryGuard {
public:
    explicit MemoryGuard(size_t bytes) {
        if (is_active) {
            throw std::runtime_error{"There is an active memory guard"};
        }
        is_active = true;
        bytes += GetDataMemoryUsage();
        if (rlimit limit{bytes, RLIM_INFINITY}; ::setrlimit(RLIMIT_DATA, &limit)) {
            throw std::system_error{errno, std::generic_category()};
        }
    }

    ~MemoryGuard() {
        is_active = false;
        rlimit limit{RLIM_INFINITY, RLIM_INFINITY};
        ::setrlimit(RLIMIT_DATA, &limit);
    }

    MemoryGuard(const MemoryGuard&) = delete;
    MemoryGuard& operator=(const MemoryGuard&) = delete;

private:
    static size_t GetDataMemoryUsage() {
        size_t pages;
        std::ifstream in{"/proc/self/statm"};
        for (auto i = 0; i < 6; ++i){
            in >> pages;
        }
        if (!in) {
            throw std::runtime_error{"Failed to get number of pages"};
        }
        return pages * kPageSize;
    } 

    static inline const auto kPageSize = ::getpagesize();
    static inline auto is_active = false;
};

template <class T = std::byte>
MemoryGuard MakeMemoryGuard(size_t n) {
    return MemoryGuard{n * sizeof(T)};
}

#endif
