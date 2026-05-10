#pragma once
// This file implements a counting semaphore using a mutex and a condition variable.
// It is used by the scheduler to limit how many training jobs can run at the same
// time. The implementation provides a blocking acquire, a non-blocking try_acquire
// that returns false immediately when no slots are free, and a release that wakes
// one waiting thread. This approach works on C++17 without requiring C++20.

#include <mutex>
#include <condition_variable>

class Semaphore {
    int count_, max_;
    std::mutex mtx_;
    std::condition_variable cv_;

public:
    explicit Semaphore(int n) : count_(n), max_(n) {}

    void acquire() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return count_ > 0; });
        --count_;
    }

    bool try_acquire() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (count_ > 0) { --count_; return true; }
        return false;
    }

    void release() {
        { std::lock_guard<std::mutex> lk(mtx_); ++count_; }
        cv_.notify_one();
    }

    int available() { std::lock_guard<std::mutex> lk(mtx_); return count_; }
    int maximum()   const { return max_; }
};
