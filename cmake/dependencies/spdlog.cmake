# ---------------------------------------------------------------------------
# spdlog — logging library
# Used by the log module and comparison benchmarks.
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

if(KITZOO_INSTALL)
    set(_spdlog_install ON)
else()
    set(_spdlog_install OFF)
endif()

# A parent project may already provide spdlog.
if(TARGET spdlog::spdlog)
    message(STATUS "Using spdlog (existing target)")
    return()
endif()

kitzoo_fetch_dependency(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    FIND_PACKAGE NAMES spdlog
    OPTIONS SPDLOG_BUILD_EXAMPLE=OFF SPDLOG_BUILD_TESTS=OFF SPDLOG_INSTALL=${_spdlog_install})

message(STATUS "Using spdlog")
