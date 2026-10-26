#include <appleboot/appleboot.hpp>

using namespace appleboot;

// @Service
class GreetingService {
public:
    std::string greet(std::string_view name) const { return "Hello, " + std::string(name) + "!"; }
};

// @RestController
class HelloController {
    GreetingService& greetings;  // constructor injection (@Autowired)

public:
    explicit HelloController(GreetingService& g) : greetings(g) {}

    Response hello(Request& req) const {
        return Response::text(greetings.greet(req.param("name")));
    }

    Response echo(Request& req) {
        return Response::json(std::string(req.body), 201);
    }

    Response add(Request& req) const {
        int a = 0, b = 0;
        std::from_chars(req.query("a").data(), req.query("a").data() + req.query("a").size(), a);
        std::from_chars(req.query("b").data(), req.query("b").data() + req.query("b").size(), b);
        return Response::json("{\"sum\":" + std::to_string(a + b) + "}");
    }

    REST_CONTROLLER(HelloController,
        GET_MAPPING("/hello/{name}", hello),
        POST_MAPPING("/echo", echo),
        GET_MAPPING("/add", add))
};

int main() {
    Application app;
    app.component<GreetingService>()
       .controller<HelloController>()
       .get("/ping", [](Request&) { return Response::text("pong"); })
       .get("/version", [](Request&) {
           return Response::json("{\"framework\":\"AppleBoot\",\"version\":\"" + std::string(appleboot::version) + "\"}");
       });
    app.run(8080);
}
