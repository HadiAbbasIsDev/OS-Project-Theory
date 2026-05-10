#pragma once
// This file contains the three functions that build JSON response bodies for
// the REST API. Each function reads from the global scheduler, resource table,
// or logger to assemble a JSON string that gets sent back to the frontend.
// build_jobs_json returns the full job list with timing and progress fields.
// build_resources_json returns the current GPU and CPU pool state along with
// the rolling utilisation history. build_logs_json returns the most recent
// log entries for the dashboard log viewer.

#include <sstream>
#include <string>
#include <algorithm>
#include "Scheduler.h"
#include "ResourceTable.h"
#include "Logger.h"
#include "JsonHelpers.h"

inline std::string build_jobs_json() {
    auto jobs = g_sched.all_jobs();
    long long cur = now_ms();

    int n_pending = 0, n_running = 0, n_completed = 0, n_preempted = 0;

    std::ostringstream o;
    o << "{\"jobs\":[";
    bool first = true;

    for (auto& j : jobs) {
        if (!first) o << ',';
        first = false;

        JobStatus st = j->status.load();
        switch (st) {
            case JobStatus::PENDING:   ++n_pending;   break;
            case JobStatus::RUNNING:   ++n_running;   break;
            case JobStatus::COMPLETED: ++n_completed; break;
            case JobStatus::PREEMPTED: ++n_preempted; break;
        }

        long long wait_ms = 0, run_ms = 0;
        int progress = 0;

        if (st == JobStatus::PENDING || st == JobStatus::PREEMPTED) {
            wait_ms = cur - j->submit_ms;
        } else if (st == JobStatus::RUNNING) {
            wait_ms = j->start_ms - j->submit_ms;
            run_ms  = cur - j->start_ms;
            progress = (int)std::min(99LL, run_ms / (j->est_duration * 10LL));
        } else {
            wait_ms = j->start_ms - j->submit_ms;
            run_ms  = j->end_ms   - j->start_ms;
            progress = 100;
        }

        o << '{'
          << "\"id\":"           << j->id                       << ','
          << "\"name\":"         << jstr(j->name)               << ','
          << "\"model_type\":"   << jstr(j->model_type)         << ','
          << "\"gpu_slots\":"    << j->gpu_slots                << ','
          << "\"cpu_cores\":"    << j->cpu_cores                << ','
          << "\"priority\":"     << jstr(prio_str(j->priority)) << ','
          << "\"status\":"       << jstr(stat_str(st))          << ','
          << "\"aging_boost\":"  << j->aging_boost              << ','
          << "\"est_duration\":" << j->est_duration             << ','
          << "\"submit_ms\":"    << j->submit_ms                << ','
          << "\"start_ms\":"     << j->start_ms                 << ','
          << "\"end_ms\":"       << j->end_ms                   << ','
          << "\"wait_ms\":"      << wait_ms                     << ','
          << "\"run_ms\":"       << run_ms                      << ','
          << "\"progress\":"     << progress
          << '}';
    }

    o << "],"
      << "\"stats\":{"
      << "\"total\":"     << (int)jobs.size() << ','
      << "\"pending\":"   << n_pending        << ','
      << "\"running\":"   << n_running        << ','
      << "\"completed\":" << n_completed      << ','
      << "\"preempted\":" << n_preempted
      << "}}";
    return o.str();
}

inline std::string build_resources_json() {
    auto rs   = g_res.get_state();
    auto snap = g_res.history;

    std::ostringstream o;
    o << "{"
      << "\"total_gpu\":"  << rs.total_gpu            << ','
      << "\"avail_gpu\":"  << rs.avail_gpu            << ','
      << "\"total_cpu\":"  << rs.total_cpu            << ','
      << "\"avail_cpu\":"  << rs.avail_cpu            << ','
      << "\"sem_max\":"    << g_sched.sem_max()       << ','
      << "\"sem_avail\":"  << g_sched.sem_available() << ','
      << "\"history\":[";

    bool first = true;
    for (auto& h : snap) {
        if (!first) o << ',';
        first = false;
        o << "{\"ts\":"  << h.ts
          << ",\"gpu\":" << h.gpu_used
          << ",\"cpu\":" << h.cpu_used << '}';
    }
    o << "]}";
    return o.str();
}

inline std::string build_logs_json() {
    auto logs = g_logger.recent(100);
    std::ostringstream o;
    o << "{\"logs\":[";
    bool first = true;
    for (auto& l : logs) {
        if (!first) o << ',';
        first = false;
        o << jstr(l);
    }
    o << "]}";
    return o.str();
}
