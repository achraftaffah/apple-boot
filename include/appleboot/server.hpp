#pragma once
// Linux epoll server: one thread per core, each with its own SO_REUSEPORT
// listening socket (shared-nothing accept path), keep-alive + pipelining.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "router.hpp"

namespace appleboot {

class Server {
    const Router& router_;

    struct Conn {
        std::string in, out;
        size_t out_off{0};
        Request req;
        bool close_after{false};
        bool want_out{false};
    };

    struct Worker {
        const Router& router;
        int lfd{-1}, ep{-1};
        std::unordered_map<int, Conn> conns;

        explicit Worker(const Router& r) : router(r) {}

        void run(uint16_t port) {
            lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (lfd < 0) throw std::runtime_error("socket() failed");
            int one = 1;
            ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
            ::setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(port);
            if (::bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0)
                throw std::runtime_error(std::string("bind() failed: ") + std::strerror(errno));
            if (::listen(lfd, 1024) < 0) throw std::runtime_error("listen() failed");

            ep = ::epoll_create1(0);
            epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = lfd;
            ::epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &ev);

            epoll_event evs[256];
            for (;;) {
                int n = ::epoll_wait(ep, evs, 256, -1);
                if (n < 0) { if (errno == EINTR) continue; break; }
                for (int i = 0; i < n; ++i) {
                    int fd = evs[i].data.fd;
                    if (fd == lfd) { accept_all(); continue; }
                    auto it = conns.find(fd);
                    if (it == conns.end()) continue;
                    uint32_t e = evs[i].events;
                    bool keep = !(e & (EPOLLERR | EPOLLHUP));
                    if (keep && (e & EPOLLIN)) keep = on_readable(it->second, fd);
                    else if (keep && (e & EPOLLOUT)) keep = flush(it->second, fd);
                    if (!keep) close_conn(fd);
                }
            }
        }

        void accept_all() {
            for (;;) {
                int cfd = ::accept4(lfd, nullptr, nullptr, SOCK_NONBLOCK);
                if (cfd < 0) return;  // EAGAIN or error
                int one = 1;
                ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                epoll_event ev{};
                ev.events = EPOLLIN | EPOLLRDHUP;
                ev.data.fd = cfd;
                ::epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &ev);
                conns.try_emplace(cfd);
            }
        }

        void close_conn(int fd) {
            ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
            ::close(fd);
            conns.erase(fd);
        }

        bool on_readable(Conn& c, int fd) {
            char buf[16384];
            for (;;) {
                ssize_t n = ::recv(fd, buf, sizeof buf, 0);
                if (n > 0) {
                    c.in.append(buf, static_cast<size_t>(n));
                    if (static_cast<size_t>(n) < sizeof buf) break;
                    continue;
                }
                if (n == 0) return false;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                return false;
            }
            process(c);
            return flush(c, fd);
        }

        void process(Conn& c) {
            size_t off = 0;
            while (off < c.in.size()) {
                size_t used = 0;
                auto r = parse_request(std::string_view(c.in).substr(off), c.req, used);
                if (r == Parse::Incomplete) break;
                if (r == Parse::Bad) {
                    Response::text("Bad Request", 400).serialize(c.out, false);
                    c.close_after = true;
                    off = c.in.size();
                    break;
                }
                Response res;
                dispatch(c.req, res);
                res.serialize(c.out, c.req.keep_alive);
                off += used;
                if (!c.req.keep_alive) { c.close_after = true; off = c.in.size(); break; }
            }
            c.in.erase(0, off);
        }

        void dispatch(Request& req, Response& res) {
            const Handler* h = router.match(req.method, req.path, req);
            if (!h) { res = Response::text("Not Found", 404); return; }
            try {
                (*h)(req, res);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "handler error: %s\n", e.what());
                res = Response::text("Internal Server Error", 500);
            }
        }

        void set_epollout(int fd, bool on) {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLRDHUP | (on ? static_cast<uint32_t>(EPOLLOUT) : 0u);
            ev.data.fd = fd;
            ::epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev);
        }

        // Returns false if the connection should be closed.
        bool flush(Conn& c, int fd) {
            while (c.out_off < c.out.size()) {
                ssize_t n = ::send(fd, c.out.data() + c.out_off, c.out.size() - c.out_off, MSG_NOSIGNAL);
                if (n > 0) { c.out_off += static_cast<size_t>(n); continue; }
                if (n < 0 && errno == EINTR) continue;
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    if (!c.want_out) { c.want_out = true; set_epollout(fd, true); }
                    return true;
                }
                return false;
            }
            c.out.clear();
            c.out_off = 0;
            if (c.want_out) { c.want_out = false; set_epollout(fd, false); }
            return !c.close_after;
        }
    };

public:
    explicit Server(const Router& r) : router_(r) {}

    void run(uint16_t port, unsigned threads) {
        if (threads == 0) threads = 1;
        std::vector<std::thread> ts;
        for (unsigned i = 0; i < threads; ++i)
            ts.emplace_back([this, port] {
                try { Worker(router_).run(port); }
                catch (const std::exception& e) { std::fprintf(stderr, "worker failed: %s\n", e.what()); }
            });
        for (auto& t : ts) t.join();
    }
};

}  // namespace appleboot
