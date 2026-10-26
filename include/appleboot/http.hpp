#pragma once
// HTTP types + zero-copy HTTP/1.1 request parser.
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace appleboot {

enum class Method : uint8_t { Get, Post, Put, Delete, Patch, Head, Options, Unknown };

inline Method parse_method(std::string_view m) {
    if (m == "GET") return Method::Get;
    if (m == "POST") return Method::Post;
    if (m == "PUT") return Method::Put;
    if (m == "DELETE") return Method::Delete;
    if (m == "PATCH") return Method::Patch;
    if (m == "HEAD") return Method::Head;
    if (m == "OPTIONS") return Method::Options;
    return Method::Unknown;
}

inline bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

using KV = std::pair<std::string_view, std::string_view>;

// All string_views point into the connection buffer and are valid only
// while the handler runs. Copy them if you need to keep them.
struct Request {
    Method method{Method::Unknown};
    std::string_view target, path, query_string, body;
    std::vector<KV> headers;
    std::vector<KV> params;  // path variables, filled by the router
    bool keep_alive{true};

    void reset() {
        method = Method::Unknown;
        target = path = query_string = body = {};
        headers.clear();  // keeps capacity -> no allocation on the hot path
        params.clear();
        keep_alive = true;
    }

    std::string_view header(std::string_view name) const {
        for (auto& [k, v] : headers) if (iequals(k, name)) return v;
        return {};
    }
    std::string_view param(std::string_view name) const {
        for (auto& [k, v] : params) if (k == name) return v;
        return {};
    }
    // Raw (not URL-decoded) query parameter lookup.
    std::string_view query(std::string_view name) const {
        std::string_view q = query_string;
        while (!q.empty()) {
            auto amp = q.find('&');
            auto pair = q.substr(0, amp);
            auto eq = pair.find('=');
            if (pair.substr(0, eq) == name)
                return eq == std::string_view::npos ? std::string_view{} : pair.substr(eq + 1);
            if (amp == std::string_view::npos) break;
            q.remove_prefix(amp + 1);
        }
        return {};
    }
};

struct Response {
    int status{200};
    std::string_view content_type{"text/plain; charset=utf-8"};  // use literals only
    std::string body;

    static Response text(std::string b, int st = 200) { return {st, "text/plain; charset=utf-8", std::move(b)}; }
    static Response json(std::string b, int st = 200) { return {st, "application/json", std::move(b)}; }
    static Response status_only(int st) { return {st, "text/plain; charset=utf-8", {}}; }

    static std::string_view reason(int s) {
        switch (s) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            default:  return "OK";
        }
    }

    void serialize(std::string& out, bool keep_alive) const {
        out += "HTTP/1.1 ";
        out += std::to_string(status);
        out += ' ';
        out += reason(status);
        out += "\r\nContent-Type: ";
        out += content_type;
        out += "\r\nContent-Length: ";
        out += std::to_string(body.size());
        out += keep_alive ? "\r\nConnection: keep-alive\r\n\r\n" : "\r\nConnection: close\r\n\r\n";
        out += body;
    }
};

enum class Parse { Incomplete, Ok, Bad };

inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

constexpr size_t kMaxHeaderBytes = 64 * 1024;
constexpr size_t kMaxBodyBytes = 1 * 1024 * 1024;

// Parses one request from `buf` without copying. On Ok, `consumed` = bytes used.
// Starter limitation: no chunked request bodies.
inline Parse parse_request(std::string_view buf, Request& r, size_t& consumed) {
    constexpr auto npos = std::string_view::npos;
    r.reset();
    auto hend = buf.find("\r\n\r\n");
    if (hend == npos) return buf.size() > kMaxHeaderBytes ? Parse::Bad : Parse::Incomplete;

    std::string_view head = buf.substr(0, hend);
    auto eol = head.find("\r\n");
    std::string_view line = head.substr(0, eol);

    auto sp1 = line.find(' ');
    if (sp1 == npos) return Parse::Bad;
    auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == npos) return Parse::Bad;

    r.method = parse_method(line.substr(0, sp1));
    r.target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    r.keep_alive = line.substr(sp2 + 1) == "HTTP/1.1";
    auto qm = r.target.find('?');
    r.path = r.target.substr(0, qm);
    if (qm != npos) r.query_string = r.target.substr(qm + 1);

    size_t content_length = 0;
    size_t pos = (eol == npos) ? head.size() : eol + 2;
    while (pos < head.size()) {
        auto next = head.find("\r\n", pos);
        std::string_view l = head.substr(pos, next == npos ? npos : next - pos);
        pos = (next == npos) ? head.size() : next + 2;
        auto colon = l.find(':');
        if (colon == npos) return Parse::Bad;
        auto name = l.substr(0, colon);
        auto value = trim(l.substr(colon + 1));
        r.headers.emplace_back(name, value);
        if (iequals(name, "content-length")) {
            auto [p, ec] = std::from_chars(value.data(), value.data() + value.size(), content_length);
            if (ec != std::errc{} || content_length > kMaxBodyBytes) return Parse::Bad;
        } else if (iequals(name, "connection")) {
            if (iequals(value, "close")) r.keep_alive = false;
            else if (iequals(value, "keep-alive")) r.keep_alive = true;
        } else if (iequals(name, "transfer-encoding")) {
            return Parse::Bad;  // chunked not supported yet
        }
    }

    size_t body_start = hend + 4;
    if (buf.size() < body_start + content_length) return Parse::Incomplete;
    r.body = buf.substr(body_start, content_length);
    consumed = body_start + content_length;
    return Parse::Ok;
}

}  // namespace appleboot
