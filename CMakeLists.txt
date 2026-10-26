cmake_minimum_required(VERSION 3.21)

# Dynamic version (git tags -> VERSION file fallback). Must come before project().
include(cmake/GitVersion.cmake)

project(AppleBoot
  VERSION ${APPLEBOOT_VERSION_NUMERIC}
  DESCRIPTION "Performance-first, Spring-style web framework for C++20"
  LANGUAGES CXX)

option(APPLEBOOT_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(APPLEBOOT_BUILD_TESTS    "Build tests"    ${PROJECT_IS_TOP_LEVEL})
option(APPLEBOOT_INSTALL        "Generate install rules" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)
include(GNUInstallDirs)

configure_file(cmake/version_gen.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/appleboot/version_gen.hpp" @ONLY)

add_library(appleboot INTERFACE)
add_library(appleboot::appleboot ALIAS appleboot)
target_compile_features(appleboot INTERFACE cxx_std_20)
target_link_libraries(appleboot INTERFACE Threads::Threads)
target_include_directories(appleboot INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(APPLEBOOT_BUILD_EXAMPLES)
  add_executable(appleboot_demo examples/main.cpp)
  target_link_libraries(appleboot_demo PRIVATE appleboot::appleboot)
  target_compile_options(appleboot_demo PRIVATE -Wall -Wextra)
endif()

if(APPLEBOOT_BUILD_TESTS)
  enable_testing()
  add_executable(appleboot_tests tests/test_main.cpp)
  target_link_libraries(appleboot_tests PRIVATE appleboot::appleboot)
  target_compile_options(appleboot_tests PRIVATE -Wall -Wextra)
  add_test(NAME appleboot_tests COMMAND appleboot_tests)
endif()

if(APPLEBOOT_INSTALL)
  include(CMakePackageConfigHelpers)
  set(_cfg_dir "${CMAKE_INSTALL_LIBDIR}/cmake/AppleBoot")

  install(TARGETS appleboot EXPORT AppleBootTargets)
  install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/appleboot/version_gen.hpp"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/appleboot")
  install(EXPORT AppleBootTargets NAMESPACE appleboot:: DESTINATION "${_cfg_dir}")

  # 0.x: minor bumps may break the API; from 1.0 on, same major is compatible.
  if(APPLEBOOT_VERSION_MAJOR EQUAL 0)
    set(_compat SameMinorVersion)
  else()
    set(_compat SameMajorVersion)
  endif()
  write_basic_package_version_file(AppleBootConfigVersion.cmake
    VERSION ${APPLEBOOT_VERSION_NUMERIC} COMPATIBILITY ${_compat} ARCH_INDEPENDENT)
  configure_package_config_file(cmake/AppleBootConfig.cmake.in AppleBootConfig.cmake
    INSTALL_DESTINATION "${_cfg_dir}")
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AppleBootConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AppleBootConfigVersion.cmake"
    DESTINATION "${_cfg_dir}")
endif()
