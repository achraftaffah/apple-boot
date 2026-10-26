# Dynamic versioning for AppleBoot.
# Include this BEFORE project(). It sets:
#   APPLEBOOT_VERSION_FULL     e.g. "0.1.0" or "0.1.0-dev.5+gabc1234.dirty"
#   APPLEBOOT_VERSION_NUMERIC  "X.Y.Z" (what project(VERSION) / find_package need)
#   APPLEBOOT_VERSION_MAJOR/MINOR/PATCH, APPLEBOOT_GIT_SHA
#
# Source of truth, in priority order:
#   1. -DAPPLEBOOT_VERSION_OVERRIDE=X.Y.Z[-pre]    (packagers, CI)
#   2. git tag "vX.Y.Z" via `git describe`          (dynamic, normal development)
#   3. the VERSION file                             (tarballs / no git / no tags)

set(_ab_root "${CMAKE_CURRENT_LIST_DIR}/..")

file(READ "${_ab_root}/VERSION" _ab_file_version)
string(STRIP "${_ab_file_version}" _ab_file_version)

set(_ab_full "${_ab_file_version}")
set(_ab_sha "unknown")

if(APPLEBOOT_VERSION_OVERRIDE)
  set(_ab_full "${APPLEBOOT_VERSION_OVERRIDE}")
elseif(EXISTS "${_ab_root}/.git")  # only trust git if THIS dir is a repo root
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
      WORKING_DIRECTORY "${_ab_root}" OUTPUT_VARIABLE _ab_head
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _ab_rc_head)
    if(_ab_rc_head EQUAL 0)
      set(_ab_sha "${_ab_head}")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
      WORKING_DIRECTORY "${_ab_root}" OUTPUT_VARIABLE _ab_status
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    set(_ab_dirty FALSE)
    if(_ab_status)
      set(_ab_dirty TRUE)
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --long
      WORKING_DIRECTORY "${_ab_root}" OUTPUT_VARIABLE _ab_desc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _ab_rc_desc)

    # v1.2.3-rc.1-5-gabc1234   ->  1.2.3 / -rc.1 / 5 / abc1234
    if(_ab_rc_desc EQUAL 0 AND
       _ab_desc MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-.+)?-([0-9]+)-g([0-9a-f]+)$")
      set(_ab_base "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
      set(_ab_pre "${CMAKE_MATCH_4}")
      set(_ab_commits "${CMAKE_MATCH_5}")
      set(_ab_sha "${CMAKE_MATCH_6}")
      set(_ab_full "${_ab_base}${_ab_pre}")
      if(_ab_commits GREATER 0)
        if(_ab_pre)
          string(APPEND _ab_full ".dev.${_ab_commits}")
        else()
          string(APPEND _ab_full "-dev.${_ab_commits}")
        endif()
      endif()
      if(_ab_commits GREATER 0 OR _ab_dirty)
        string(APPEND _ab_full "+g${_ab_sha}")
        if(_ab_dirty)
          string(APPEND _ab_full ".dirty")
        endif()
      endif()
    else()
      # git repo without a release tag: VERSION file + build metadata
      string(APPEND _ab_full "+g${_ab_sha}")
      if(_ab_dirty)
        string(APPEND _ab_full ".dirty")
      endif()
    endif()

    # Re-run CMake when HEAD moves so the version stays fresh.
    if(EXISTS "${_ab_root}/.git/HEAD")
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ab_root}/.git/HEAD")
    endif()
  endif()
endif()

if(NOT _ab_full MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  message(FATAL_ERROR "AppleBoot: cannot parse version '${_ab_full}' (expected X.Y.Z[-pre][+meta])")
endif()
set(APPLEBOOT_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(APPLEBOOT_VERSION_MINOR "${CMAKE_MATCH_2}")
set(APPLEBOOT_VERSION_PATCH "${CMAKE_MATCH_3}")
set(APPLEBOOT_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(APPLEBOOT_VERSION_FULL "${_ab_full}")
set(APPLEBOOT_GIT_SHA "${_ab_sha}")

message(STATUS "AppleBoot version: ${APPLEBOOT_VERSION_FULL}")
