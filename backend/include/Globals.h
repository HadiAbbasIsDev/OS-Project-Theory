#pragma once
// This file declares the process-wide shutdown flag. When the process receives
// SIGINT or SIGTERM the signal handler sets this flag to false, which causes
// the HTTP accept loop and all background scheduler threads to exit on their
// next iteration without needing any additional signaling mechanism.

#include <atomic>

inline std::atomic<bool> g_running{true};
