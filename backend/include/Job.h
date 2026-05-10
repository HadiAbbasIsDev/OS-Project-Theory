#pragma once
// This file defines the Job struct along with the Priority and JobStatus enumerations
// used throughout the scheduler. Status and the preemption flag are stored as atomic
// fields so any thread can read them without needing to hold a lock. The aging boost
// field is modified only by the aging loop, which already holds the scheduler mutex,
// so it does not need to be atomic. The file also provides string conversion helpers
// for both enumerations and a small utility that returns the current time in milliseconds.

#include <string>
#include <atomic>
#include <chrono>

enum class Priority  { HIGH = 0, MEDIUM = 1, LOW = 2 };
enum class JobStatus { PENDING, RUNNING, COMPLETED, PREEMPTED };

inline const char* prio_str(Priority p) {
    switch (p) {
        case Priority::HIGH:   return "High";
        case Priority::MEDIUM: return "Medium";
        default:               return "Low";
    }
}

inline const char* stat_str(JobStatus s) {
    switch (s) {
        case JobStatus::PENDING:   return "Pending";
        case JobStatus::RUNNING:   return "Running";
        case JobStatus::COMPLETED: return "Completed";
        default:                   return "Preempted";
    }
}

inline long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

struct Job {
    int id;
    std::string name, model_type;
    int gpu_slots, cpu_cores;
    Priority priority;
    int est_duration;

    std::atomic<JobStatus> status{JobStatus::PENDING};
    long long submit_ms{0}, start_ms{0}, end_ms{0};
    std::atomic<bool> preempt{false};
    int aging_boost{0};

    int effective_priority() const {
        return std::max(0, static_cast<int>(priority) - aging_boost);
    }

    Job(int id_, const std::string& n, const std::string& mt,
        int gpu, int cpu, Priority p, int dur)
        : id(id_), name(n), model_type(mt),
          gpu_slots(gpu), cpu_cores(cpu),
          priority(p), est_duration(dur) {}
};
