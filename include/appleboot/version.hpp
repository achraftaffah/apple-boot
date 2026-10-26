#pragma once
// Version information. When built through CMake, the real values come from
// the generated "appleboot/version_gen.hpp" (derived from git tags / VERSION).
// Without CMake (plain g++ -Iinclude) the fallback below is used.
#include <string_view>

#if __has_include("appleboot/version_gen.hpp")
#include "appleboot/version_gen.hpp"
#else
#define APPLEBOOT_VERSION_MAJOR 0
#define APPLEBOOT_VERSION_MINOR 0
#define APPLEBOOT_VERSION_PATCH 0
#define APPLEBOOT_VERSION_STRING "0.0.0-unknown"
#define APPLEBOOT_GIT_SHA "unknown"
#endif

#define APPLEBOOT_VERSION_AT_LEAST(maj, min, pat)                                       \
    ((APPLEBOOT_VERSION_MAJOR * 1000000 + APPLEBOOT_VERSION_MINOR * 1000 +              \
      APPLEBOOT_VERSION_PATCH) >= ((maj) * 1000000 + (min) * 1000 + (pat)))

namespace appleboot {
inline constexpr int version_major = APPLEBOOT_VERSION_MAJOR;
inline constexpr int version_minor = APPLEBOOT_VERSION_MINOR;
inline constexpr int version_patch = APPLEBOOT_VERSION_PATCH;
inline constexpr std::string_view version = APPLEBOOT_VERSION_STRING;
inline constexpr std::string_view git_sha = APPLEBOOT_GIT_SHA;
}  // namespace appleboot
