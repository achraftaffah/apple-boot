# Changelog
All notable changes are documented here. Format: [Keep a Changelog](https://keepachangelog.com),
versioning: [Semantic Versioning](https://semver.org).

## [Unreleased]

## [0.1.0] - 2026-10-26
### Added
- Epoll HTTP/1.1 server, one thread per core (`SO_REUSEPORT`), keep-alive and pipelining.
- Zero-copy request parser based on `std::string_view`.
- Segment-trie router with `{path}` variables.
- Constructor-injection DI container (singleton scope, circular dependency detection).
- Spring-style macros: `REST_CONTROLLER`, `GET_MAPPING`, `POST_MAPPING`, `PUT_MAPPING`, `DELETE_MAPPING`, `PATCH_MAPPING`.
- Functional routes: `app.get(...)`, `app.post(...)`.
- Git-tag based dynamic versioning, `appleboot/version.hpp`, CMake package (`find_package(AppleBoot)`).

### Known limitations
- Linux only; no TLS, HTTP/2, WebSocket or chunked request bodies.
