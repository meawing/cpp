#pragma once

#include <mutex>
#include <condition_variable>

class RWLock {
public:
    void Read(auto func) {
        std::unique_lock<std::mutex> lock(global_);
        while (writers_ > 0 || writing_) {  //  Wait if there are writers or a writer is pending
            readers_cv_.wait(lock);
        }
        ++readers_;
        lock.unlock();  //  Release the global lock while reading

        try {
            func();
        } catch (...) {
            EndRead();
            throw;
        }

        EndRead();
    }

    void Write(auto func) {
        std::unique_lock<std::mutex> lock(global_);
        while (readers_ > 0 || writers_ > 0) {  //  Wait if there are readers or writers
            writers_cv_.wait(lock);
        }
        writing_ = true;
        ++writers_;
        lock.unlock();  //  Release the global lock while writing

        try {
            func();
        } catch (...) {
            EndWrite();
            throw;
        }

        EndWrite();
    }

private:
    void EndRead() {
        std::unique_lock<std::mutex> lock(global_);
        if (--readers_ == 0) {
            writers_cv_.notify_all();  //  Notify waiting writers
        }
    }

    void EndWrite() {
        std::unique_lock<std::mutex> lock(global_);
        --writers_;
        writing_ = false;
        readers_cv_.notify_all();  //  Notify waiting readers
        writers_cv_.notify_all();  //  Notify waiting writers
    }

    std::mutex global_;
    std::condition_variable readers_cv_;
    std::condition_variable writers_cv_;
    int readers_ = 0;
    int writers_ = 0;
    bool writing_ = false;
};
