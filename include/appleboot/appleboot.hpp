#pragma once
// Public API: Application, DI, and Spring-like mapping macros.
#include <functional>
#include <thread>
#include <tuple>

#include "di.hpp"
#include "http.hpp"
#include "router.hpp"
#include "server.hpp"
#include "version.hpp"

namespace appleboot {

template <class C>
struct Route {
    Method method;
    std::string_view path;
    std::function<Response(C&, Request&)> fn;
};

template <class C>
Route<C> route(Method m, std::string_view p, Response (C::*f)(Request&)) {
    return {m, p, [f](C& c, Request& r) { return (c.*f)(r); }};
}
template <class C>
Route<C> route(Method m, std::string_view p, Response (C::*f)(Request&) const) {
    return {m, p, [f](C& c, Request& r) { return (c.*f)(r); }};
}

class Application {
    Container container_;
    Router router_;

    template <class C>
    void add_route(C& obj, const Route<C>& rt) {
        router_.add(rt.method, rt.path, [&obj, fn = rt.fn](Request& rq, Response& rs) { rs = fn(obj, rq); });
    }

public:
    // @Component / @Service: eagerly create a bean (and its dependencies).
    template <class T>
    Application& component() { container_.get<T>(); return *this; }

    // @RestController: autowire the controller and register its routes.
    template <class C>
    Application& controller() {
        C& obj = container_.get<C>();
        std::apply([&](auto... r) { (add_route(obj, r), ...); }, C::routes());
        return *this;
    }

    // Functional style: app.get("/ping", [](Request&) { return Response::text("pong"); });
    template <class F>
    Application& on(Method m, std::string_view path, F f) {
        router_.add(m, path, [f = std::move(f)](Request& rq, Response& rs) { rs = f(rq); });
        return *this;
    }
    template <class F> Application& get(std::string_view p, F f)  { return on(Method::Get, p, std::move(f)); }
    template <class F> Application& post(std::string_view p, F f) { return on(Method::Post, p, std::move(f)); }

    template <class T>
    T& bean() { return container_.get<T>(); }

    // Blocks. Controllers are shared between worker threads: keep them thread-safe.
    void run(uint16_t port = 8080, unsigned threads = std::thread::hardware_concurrency()) {
        std::fprintf(stderr, "AppleBoot %.*s listening on :%u with %u threads\n",
                     static_cast<int>(version.size()), version.data(), port, threads);
        Server(router_).run(port, threads);
    }
};

}  // namespace appleboot

// ---- Spring-like syntax -----------------------------------------------------
// Place REST_CONTROLLER inside the class. Handlers: Response name(Request&) [const]
#define REST_CONTROLLER(Self, ...)                      \
    static auto routes() {                              \
        using _appleboot_self = Self;                         \
        return std::make_tuple(__VA_ARGS__);            \
    }

#define GET_MAPPING(path, fn)    ::appleboot::route(::appleboot::Method::Get,    path, &_appleboot_self::fn)
#define POST_MAPPING(path, fn)   ::appleboot::route(::appleboot::Method::Post,   path, &_appleboot_self::fn)
#define PUT_MAPPING(path, fn)    ::appleboot::route(::appleboot::Method::Put,    path, &_appleboot_self::fn)
#define DELETE_MAPPING(path, fn) ::appleboot::route(::appleboot::Method::Delete, path, &_appleboot_self::fn)
#define PATCH_MAPPING(path, fn)  ::appleboot::route(::appleboot::Method::Patch,  path, &_appleboot_self::fn)
