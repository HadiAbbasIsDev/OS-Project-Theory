#pragma once
// This file defines the Logger class and creates a global logger instance used
// by every component in the backend. The logger uses a mutex so multiple threads
// can write concurrently without corrupting the buffer. Each entry is timestamped
// down to the millisecond, appended to a log file on disk, and stored in an
// in-memory ring buffer capped at 300 entries so the frontend can fetch recent
// events through the API without reading the file.

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <algorithm>

class Logger {
    std::mutex mtx_;
    std::deque<std::string> buf_;
    std::ofstream file_;
    static constexpr int MAX_ENTRIES = 300;

public:
    Logger() : file_("scheduler.log", std::ios::app) {}

    void log(const std::string& level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()).count() % 1000;

        char tbuf[20];
        std::strftime(tbuf, sizeof(tbuf), "%H:%M:%S", std::localtime(&t));

        std::ostringstream oss;
        oss << tbuf << '.'
            << std::setfill('0') << std::setw(3) << ms
            << " [" << level << "] " << msg;
        std::string entry = oss.str();

        std::lock_guard<std::mutex> lk(mtx_);
        buf_.push_back(entry);
        if ((int)buf_.size() > MAX_ENTRIES) buf_.pop_front();
        if (file_.is_open()) { file_ << entry << '\n'; file_.flush(); }
        std::cout << entry << '\n';
    }

    std::vector<std::string> recent(int n = 100) {
        std::lock_guard<std::mutex> lk(mtx_);
        int start = std::max(0, (int)buf_.size() - n);
        return { buf_.begin() + start, buf_.end() };
    }
};

inline Logger g_logger;
