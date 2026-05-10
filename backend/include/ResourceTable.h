#pragma once
// This file defines the shared GPU and CPU resource pool and creates a global
// instance used by the scheduler and API handlers. Every read and write to the
// available counters goes through a mutex so allocate and release calls from
// different threads cannot race with each other. The table also maintains a
// rolling 60-point history of utilisation snapshots, one per second, that the
// frontend charts use to draw the GPU and CPU usage lines over time.

#include <mutex>
#include <deque>
#include <algorithm>
#include "Job.h"

struct ResSnap {
    long long ts;
    int gpu_used, cpu_used;
};

struct ResourceTable {
    static constexpr int TOTAL_GPU = 8;
    static constexpr int TOTAL_CPU = 16;

    int avail_gpu{TOTAL_GPU};
    int avail_cpu{TOTAL_CPU};
    std::mutex mtx_;
    std::deque<ResSnap> history;

    bool allocate(int gpu, int cpu) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (avail_gpu >= gpu && avail_cpu >= cpu) {
            avail_gpu -= gpu;
            avail_cpu -= cpu;
            return true;
        }
        return false;
    }

    void release(int gpu, int cpu) {
        std::lock_guard<std::mutex> lk(mtx_);
        avail_gpu = std::min(TOTAL_GPU, avail_gpu + gpu);
        avail_cpu = std::min(TOTAL_CPU, avail_cpu + cpu);
    }

    void snapshot() {
        std::lock_guard<std::mutex> lk(mtx_);
        history.push_back({ now_ms(),
                             TOTAL_GPU - avail_gpu,
                             TOTAL_CPU - avail_cpu });
        if ((int)history.size() > 60) history.pop_front();
    }

    struct State { int total_gpu, avail_gpu, total_cpu, avail_cpu; };
    State get_state() {
        std::lock_guard<std::mutex> lk(mtx_);
        return { TOTAL_GPU, avail_gpu, TOTAL_CPU, avail_cpu };
    }
};

inline ResourceTable g_res;
