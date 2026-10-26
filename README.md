# AppleBoot

Performance-first, Spring-style web framework for C++20 (Linux).

```cpp
#include <appleboot/appleboot.hpp>
using namespace appleboot;

class HelloController {
public:
    Response hello(Request& req) const { return Response::text("Hello, " + std::string(req.param("name"))); }
    REST_CONTROLLER(HelloController, GET_MAPPING("/hello/{name}", hello))
};

int main() { Application app; app.controller<HelloController>(); app.run(8080); }
```

## Build & run
```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/appleboot_demo        # curl localhost:8080/hello/Ali
```

## Use it in your project
```cmake
include(FetchContent)
FetchContent_Declare(appleboot
  GIT_REPOSITORY https://github.com/<you>/appleboot.git
  GIT_TAG v0.1.0)
FetchContent_MakeAvailable(appleboot)
target_link_libraries(myapp PRIVATE appleboot::appleboot)
```
Or install (`cmake --install build --prefix /usr/local`) and use `find_package(AppleBoot 0.1 REQUIRED)`.

## Versioning (dynamic)
The version is computed at configure time, in this order:

| Source | Example result |
|---|---|
| `-DAPPLEBOOT_VERSION_OVERRIDE=1.2.3` | `1.2.3` |
| git tag `v0.1.0` on HEAD, clean tree | `0.1.0` |
| 5 commits after the tag | `0.1.0-dev.5+g1a2b3c4` |
| uncommitted changes | `0.1.0-dev.5+g1a2b3c4.dirty` |
| no git / no tags | contents of `VERSION` (+ `+gSHA` if in git) |

In code: `appleboot::version`, `appleboot::version_major/minor/patch`, `APPLEBOOT_VERSION_AT_LEAST(0,1,0)`.
`GET /version` in the demo shows it at runtime.

### Releasing
```bash
# 1. add a "## [0.2.0] - YYYY-MM-DD" section to CHANGELOG.md
bash scripts/release.sh 0.2.0         # bumps VERSION, commits, tags v0.2.0
git push origin HEAD && git push origin v0.2.0
```
The `release` GitHub workflow then verifies tag == `VERSION`, builds, tests and publishes a GitHub Release using the changelog section as notes.
While on 0.x, minor versions may contain breaking changes.

## License
MIT
