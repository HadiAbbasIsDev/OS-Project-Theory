#pragma once
// This file defines the main scheduling engine and creates a global scheduler instance.
// It uses a semaphore to cap how many jobs run at once, a mutex to protect the job
// registry and pending queue, and detached worker threads to execute each job. Three
// background threads run continuously: one dispatches pending jobs whenever a semaphore
// slot becomes free, one applies aging boosts to jobs that have been waiting too long,
// and one records resource utilisation snapshots every second. Preemption works by
// setting an atomic flag on the lowest-priority running job when a high-priority job
// arrives and no slots are free, causing that worker to stop on its next tick and
// return the job to the pending queue.

#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <string>
#include "Globals.h"
#include "Semaphore.h"
#include "Logger.h"
#include "Job.h"
#include "ResourceTable.h"

class Scheduler {
public:
    static constexpr int MAX_CONCURRENT = 4;

    void start() {
        std::thread(&Scheduler::dispatch_loop, this).detach();
        std::thread(&Scheduler::aging_loop,    this).detach();
        std::thread(&Scheduler::snapshot_loop, this).detach();
    }

    int submit(const std::string& name, const std::string& model,
               int gpu, int cpu, Priority prio, int dur) {
        int id;
        std::shared_ptr<Job> job;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            id  = next_id_++;
            job = std::make_shared<Job>(id, name, model, gpu, cpu, prio, dur);
            job->submit_ms = now_ms();
            registry_[id]  = job;
            pending_.push_back(id);
        }
        cv_.notify_all();

        g_logger.log("SUBMIT",
            "Job #" + std::to_string(id) + " '" + name +
            "' [" + prio_str(prio) + " | " +
            std::to_string(gpu) + " GPU | " +
            std::to_string(cpu) + " CPU | ~" +
            std::to_string(dur) + "s]");

        if (prio == Priority::HIGH && sem_.available() == 0)
            try_preempt_for(id);

        return id;
    }

    std::vector<std::shared_ptr<Job>> all_jobs() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::shared_ptr<Job>> v;
        v.reserve(registry_.size());
        for (auto& [id, j] : registry_) v.push_back(j);
        return v;
    }

    int sem_available() { return sem_.available(); }
    int sem_max()       { return sem_.maximum();   }

private:
    Semaphore sem_{MAX_CONCURRENT};
    std::mutex mtx_;
    std::condition_variable cv_;
    std::map<int, std::shared_ptr<Job>> registry_;
    std::vector<int> pending_;
    int next_id_{1};

    int pick_best() {
        if (pending_.empty()) return -1;
        auto rs = g_res.get_state();

        std::vector<int> cands = pending_;
        std::sort(cands.begin(), cands.end(), [&](int a, int b) {
            auto& ja = registry_[a];
            auto& jb = registry_[b];
            int pa = ja->effective_priority(), pb = jb->effective_priority();
            return (pa != pb) ? (pa < pb) : (ja->submit_ms < jb->submit_ms);
        });

        for (int id : cands) {
            auto& j = registry_[id];
            if (j->gpu_slots <= rs.avail_gpu && j->cpu_cores <= rs.avail_cpu) {
                pending_.erase(std::find(pending_.begin(), pending_.end(), id));
                return id;
            }
        }
        return -1;
    }

    void run_job(std::shared_ptr<Job> job) {
        job->start_ms = now_ms();
        job->status.store(JobStatus::RUNNING);

        g_logger.log("START",
            "Job #" + std::to_string(job->id) + " '" + job->name +
            "' executing on " +
            std::to_string(job->gpu_slots) + " GPU / " +
            std::to_string(job->cpu_cores) + " CPU");

        int total_ticks = job->est_duration * 10;
        for (int t = 0; t < total_ticks && g_running.load(); ++t) {
            if (job->preempt.load()) {
                job->status.store(JobStatus::PREEMPTED);
                g_res.release(job->gpu_slots, job->cpu_cores);
                sem_.release();
                cv_.notify_all();

                job->preempt.store(false);
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    job->status.store(JobStatus::PENDING);
                    pending_.push_back(job->id);
                }
                g_logger.log("PREEMPT",
                    "Job #" + std::to_string(job->id) +
                    " '" + job->name + "' preempted and re-queued");
                cv_.notify_all();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!g_running.load()) return;

        job->end_ms = now_ms();
        job->status.store(JobStatus::COMPLETED);
        g_res.release(job->gpu_slots, job->cpu_cores);
        sem_.release();
        cv_.notify_all();

        long long secs = (job->end_ms - job->start_ms) / 1000;
        g_logger.log("DONE",
            "Job #" + std::to_string(job->id) +
            " '" + job->name + "' completed in " +
            std::to_string(secs) + "s (est " +
            std::to_string(job->est_duration) + "s)");
    }

    void try_preempt_for(int incoming_id) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::shared_ptr<Job> victim;
        for (auto& [id, j] : registry_) {
            if (j->status.load() == JobStatus::RUNNING) {
                if (!victim ||
                    static_cast<int>(j->priority) >
                    static_cast<int>(victim->priority))
                    victim = j;
            }
        }
        if (victim && victim->priority == Priority::LOW) {
            g_logger.log("PREEMPT",
                "Signaling job #" + std::to_string(victim->id) +
                " '" + victim->name + "' to yield for job #" +
                std::to_string(incoming_id));
            victim->preempt.store(true);
        }
    }

    void dispatch_loop() {
        while (g_running.load()) {
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait_for(lk, std::chrono::milliseconds(150));
            }
            while (g_running.load()) {
                if (!sem_.try_acquire()) break;

                std::shared_ptr<Job> job;
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    int id = pick_best();
                    if (id == -1) { sem_.release(); break; }
                    job = registry_[id];
                }

                if (!g_res.allocate(job->gpu_slots, job->cpu_cores)) {
                    std::lock_guard<std::mutex> lk(mtx_);
                    pending_.push_back(job->id);
                    sem_.release();
                    break;
                }
                std::thread([this, job]() { run_job(job); }).detach();
            }
        }
    }

    void aging_loop() {
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::lock_guard<std::mutex> lk(mtx_);
            for (int id : pending_) {
                auto& j = registry_[id];
                if (j->aging_boost < 2) {
                    ++j->aging_boost;
                    g_logger.log("AGING",
                        "Priority boost +" + std::to_string(j->aging_boost) +
                        " applied to job #" + std::to_string(id) +
                        " '" + j->name + "'");
                }
            }
        }
    }

    void snapshot_loop() {
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            g_res.snapshot();
        }
    }
};

inline Scheduler g_sched;
