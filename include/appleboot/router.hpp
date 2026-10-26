#pragma once
// Radix-style segment trie, built once at startup. Static segments win over {params}.
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "http.hpp"

namespace appleboot {

using Handler = std::function<void(Request&, Response&)>;

class Router {
    struct Node {
        std::map<std::string, std::unique_ptr<Node>, std::less<>> statics;
        std::unique_ptr<Node> param;
        std::string param_name;
        Handler handler;
    };
    std::array<Node, 8> roots_;

    template <class F>
    static void for_each_segment(std::string_view p, F&& f) {
        while (!p.empty()) {
            if (p.front() == '/') { p.remove_prefix(1); continue; }
            auto slash = p.find('/');
            if (!f(p.substr(0, slash))) return;
            if (slash == std::string_view::npos) return;
            p.remove_prefix(slash);
        }
    }

public:
    // pattern example: "/users/{id}/posts"
    void add(Method m, std::string_view pattern, Handler h) {
        Node* n = &roots_[static_cast<size_t>(m)];
        for_each_segment(pattern, [&](std::string_view seg) {
            if (seg.size() > 2 && seg.front() == '{' && seg.back() == '}') {
                if (!n->param) {
                    n->param = std::make_unique<Node>();
                    n->param_name = std::string(seg.substr(1, seg.size() - 2));
                }
                n = n->param.get();
            } else {
                auto& child = n->statics[std::string(seg)];
                if (!child) child = std::make_unique<Node>();
                n = child.get();
            }
            return true;
        });
        n->handler = std::move(h);
    }

    // Returns handler or nullptr. Fills req.params (views into the path / router).
    // Note: no backtracking between static and param branches (keeps matching O(segments)).
    const Handler* match(Method m, std::string_view path, Request& req) const {
        const Node* n = &roots_[static_cast<size_t>(m)];
        bool ok = true;
        for_each_segment(path, [&](std::string_view seg) {
            if (auto it = n->statics.find(seg); it != n->statics.end()) {
                n = it->second.get();
            } else if (n->param) {
                req.params.emplace_back(n->param_name, seg);
                n = n->param.get();
            } else {
                ok = false;
                return false;
            }
            return true;
        });
        return (ok && n->handler) ? &n->handler : nullptr;
    }
};

}  // namespace appleboot
