#pragma once
// This file provides lightweight JSON serialisation and parsing utilities used
// by the API handler functions. It includes a function to encode a C++ string
// as a properly escaped JSON string literal, a function that extracts a value
// by key from a flat JSON object string and works with both quoted string values
// and bare numeric values, and a convenience wrapper that parses the extracted
// value directly as an integer.

#include <string>
#include <algorithm>

inline std::string jstr(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else                r += c;
    }
    return r + '"';
}

inline std::string json_get_val(const std::string& json, const std::string& key) {
    std::string search = '"' + key + '"';
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        ++pos;
        size_t end = json.find('"', pos);
        return (end != std::string::npos) ? json.substr(pos, end - pos) : "";
    }
    size_t end = pos;
    while (end < json.size() &&
           json[end] != ',' && json[end] != '}' &&
           json[end] != '\n' && json[end] != '\r') ++end;
    std::string v = json.substr(pos, end - pos);
    while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
    return v;
}

inline int json_get_int(const std::string& json, const std::string& key) {
    std::string v = json_get_val(json, key);
    if (v.empty()) return 0;
    try { return std::stoi(v); } catch (...) { return 0; }
}
