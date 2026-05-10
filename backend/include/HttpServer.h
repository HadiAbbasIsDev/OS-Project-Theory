#pragma once
// This file implements a minimal HTTP/1.1 server using POSIX socket calls. It
// provides a function to read a complete request from a socket descriptor, a
// parser that extracts the method, path, and body from the raw bytes, a response
// builder that formats a proper HTTP response with CORS headers, a file server
// that reads a static file from disk and returns it with the right content type,
// and a connection handler that routes incoming requests to the correct API
// function or static file. Each connection is handled on its own thread spawned
// by the accept loop in main.

#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ApiHandlers.h"

inline std::string read_http_request(int fd) {
    std::string data;
    data.reserve(4096);
    char buf[4096];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        data.append(buf, n);

        size_t hend = data.find("\r\n\r\n");
        if (hend == std::string::npos) continue;

        std::string lower_hdr = data.substr(0, hend);
        std::transform(lower_hdr.begin(), lower_hdr.end(),
                       lower_hdr.begin(), ::tolower);
        size_t cl = lower_hdr.find("content-length:");
        if (cl != std::string::npos) {
            size_t vs = cl + 15;
            while (vs < lower_hdr.size() && lower_hdr[vs] == ' ') ++vs;
            size_t ve = lower_hdr.find('\r', vs);
            int body_len = std::stoi(lower_hdr.substr(vs, ve - vs));
            if ((int)data.size() >= (int)(hend + 4 + body_len)) break;
        } else {
            break;
        }
    }
    return data;
}

struct HttpReq {
    std::string method, path, body;
};

inline HttpReq parse_http(const std::string& raw) {
    HttpReq req;
    std::istringstream ss(raw);
    std::string line;

    std::getline(ss, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream rl(line);
    rl >> req.method >> req.path;

    size_t qs = req.path.find('?');
    if (qs != std::string::npos) req.path = req.path.substr(0, qs);

    int content_len = 0;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.rfind("content-length:", 0) == 0)
            content_len = std::stoi(line.substr(15));
    }

    size_t bstart = raw.find("\r\n\r\n");
    if (bstart != std::string::npos && content_len > 0)
        req.body = raw.substr(bstart + 4, content_len);

    return req;
}

inline std::string http_response(int code, const std::string& ct,
                                  const std::string& body) {
    const char* reason = (code == 200) ? "OK"          :
                         (code == 201) ? "Created"     :
                         (code == 400) ? "Bad Request" : "Not Found";
    std::ostringstream o;
    o << "HTTP/1.1 " << code << ' ' << reason << "\r\n"
      << "Content-Type: "   << ct          << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Access-Control-Allow-Headers: Content-Type\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return o.str();
}

inline std::string serve_file(const std::string& path, const std::string& ct) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return http_response(404, "text/plain", "File not found: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return http_response(200, ct, ss.str());
}

inline void handle_client(int fd) {
    std::string raw = read_http_request(fd);
    if (raw.empty()) { close(fd); return; }

    HttpReq req = parse_http(raw);
    std::string resp;

    if (req.method == "OPTIONS") {
        resp = "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Content-Length: 0\r\n"
               "Connection: close\r\n\r\n";

    } else if (req.method == "GET") {
        if      (req.path == "/" || req.path == "/index.html")
            resp = serve_file("frontend/index.html", "text/html");
        else if (req.path == "/style.css")
            resp = serve_file("frontend/style.css",  "text/css");
        else if (req.path == "/app.js")
            resp = serve_file("frontend/app.js",     "application/javascript");
        else if (req.path == "/api/jobs")
            resp = http_response(200, "application/json", build_jobs_json());
        else if (req.path == "/api/resources")
            resp = http_response(200, "application/json", build_resources_json());
        else if (req.path == "/api/logs")
            resp = http_response(200, "application/json", build_logs_json());
        else
            resp = http_response(404, "text/plain", "Not found");

    } else if (req.method == "POST" && req.path == "/api/jobs") {
        std::string name   = json_get_val(req.body, "name");
        std::string model  = json_get_val(req.body, "model_type");
        std::string prio_s = json_get_val(req.body, "priority");
        int gpu = json_get_int(req.body, "gpu_slots");
        int cpu = json_get_int(req.body, "cpu_cores");
        int dur = json_get_int(req.body, "estimated_duration");

        if (name.empty() || gpu <= 0 || cpu <= 0 || dur <= 0) {
            resp = http_response(400, "application/json",
                                 "{\"error\":\"Invalid job parameters\"}");
        } else {
            Priority prio = Priority::MEDIUM;
            if      (prio_s == "High") prio = Priority::HIGH;
            else if (prio_s == "Low")  prio = Priority::LOW;

            gpu = std::min(gpu, ResourceTable::TOTAL_GPU);
            cpu = std::min(cpu, ResourceTable::TOTAL_CPU);

            int id = g_sched.submit(name, model, gpu, cpu, prio, dur);
            resp = http_response(201, "application/json",
                "{\"success\":true,\"job_id\":" + std::to_string(id) + "}");
        }
    } else {
        resp = http_response(404, "text/plain", "Not found");
    }

    send(fd, resp.c_str(), resp.size(), MSG_NOSIGNAL);
    close(fd);
}
