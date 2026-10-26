// Minimal dependency-free test runner.
#include <appleboot/appleboot.hpp>

#include <cstdio>
#include <string>

using namespace appleboot;

static int failures = 0;
#define CHECK(c)                                                                 \
    do {                                                                         \
        if (!(c)) { std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } \
    } while (0)

static void test_parser() {
    std::string raw = "GET /users/42?x=1&y=2 HTTP/1.1\r\nHost: a\r\nConnection: keep-alive\r\n\r\n";
    Request r; size_t used = 0;
    CHECK(parse_request(raw, r, used) == Parse::Ok);
    CHECK(used == raw.size());
    CHECK(r.method == Method::Get);
    CHECK(r.path == "/users/42");
    CHECK(r.query("y") == "2");
    CHECK(r.header("host") == "a");
    CHECK(parse_request(raw.substr(0, raw.size() - 4), r, used) == Parse::Incomplete);

    std::string post = "POST /e HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel";
    CHECK(parse_request(post, r, used) == Parse::Incomplete);
    post += "lo";
    CHECK(parse_request(post, r, used) == Parse::Ok);
    CHECK(r.body == "hello");

    std::string chunked = "POST /e HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
    CHECK(parse_request(chunked, r, used) == Parse::Bad);
}

static void test_router() {
    Router router;
    int hit = 0;
    router.add(Method::Get, "/users/{id}/posts", [&](Request&, Response&) { hit = 1; });
    router.add(Method::Get, "/users/me", [&](Request&, Response&) { hit = 2; });
    Request req; Response res;

    const Handler* h = router.match(Method::Get, "/users/42/posts", req);
    CHECK(h != nullptr);
    CHECK(req.param("id") == "42");
    (*h)(req, res);
    CHECK(hit == 1);

    req.reset();
    h = router.match(Method::Get, "/users/me", req);
    CHECK(h != nullptr);
    (*h)(req, res);
    CHECK(hit == 2);  // static segment wins over {id}

    req.reset();
    CHECK(router.match(Method::Post, "/users/me", req) == nullptr);
    CHECK(router.match(Method::Get, "/nope", req) == nullptr);
}

struct A {};
struct B { A& a; explicit B(A& x) : a(x) {} };
struct C { A& a; B& b; C(A& x, B& y) : a(x), b(y) {} };

static void test_di() {
    Container c;
    C& cc = c.get<C>();
    CHECK(&cc.a == &c.get<A>());
    CHECK(&cc.b == &c.get<B>());
    CHECK(&cc.b.a == &cc.a);  // singleton scope
}

static void test_version() {
    CHECK(!version.empty());
    CHECK(APPLEBOOT_VERSION_AT_LEAST(0, 0, 0));
    std::string prefix = std::to_string(version_major) + "." + std::to_string(version_minor) + "." +
                         std::to_string(version_patch);
    CHECK(std::string(version).rfind(prefix, 0) == 0);
}

int main() {
    test_parser();
    test_router();
    test_di();
    test_version();
    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    std::puts("all tests passed");
    return 0;
}
