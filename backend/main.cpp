// This is the entry point for the scheduler process. It registers signal handlers
// for a clean shutdown on SIGINT and SIGTERM, starts the global scheduler threads,
// opens a TCP socket on port 8080, and runs an accept loop that hands each
// incoming connection off to a detached thread through handle_client.

#include <csignal>
#include <cstdio>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "include/Globals.h"
#include "include/Semaphore.h"
#include "include/Logger.h"
#include "include/Job.h"
#include "include/ResourceTable.h"
#include "include/Scheduler.h"
#include "include/JsonHelpers.h"
#include "include/ApiHandlers.h"
#include "include/HttpServer.h"

static void sig_handler(int) { g_running.store(false); }

int main() {
    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);

    g_logger.log("INFO", "AI Training Job Scheduler starting");
    g_logger.log("INFO",
        "Resources: " +
        std::to_string(ResourceTable::TOTAL_GPU) + " GPU slots, " +
        std::to_string(ResourceTable::TOTAL_CPU) + " CPU cores");
    g_logger.log("INFO",
        "Semaphore N = " + std::to_string(Scheduler::MAX_CONCURRENT) +
        " (max concurrent training jobs)");

    g_sched.start();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(8080);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, 64) < 0) { perror("listen"); return 1; }

    g_logger.log("INFO", "HTTP server listening on http://localhost:8080");
    std::puts("\n  Dashboard -> http://localhost:8080\n"
              "  Press Ctrl+C to stop.\n");

    while (g_running.load()) {
        sockaddr_in client{};
        socklen_t   clen = sizeof(client);
        int cfd = accept(server_fd, (sockaddr*)&client, &clen);
        if (cfd < 0) continue;
        std::thread([cfd]() { handle_client(cfd); }).detach();
    }

    close(server_fd);
    g_logger.log("INFO", "Scheduler shut down cleanly.");
    return 0;
}
