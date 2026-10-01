# ---------------------------------------------------------------------------
# cxxopts — command-line option parsing
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

kitzoo_fetch_dependency(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.3.1
    OPTIONS CXXOPTS_BUILD_EXAMPLES=OFF CXXOPTS_BUILD_TESTS=OFF
            CXXOPTS_ENABLE_INSTALL=${KITZOO_INSTALL} CXXOPTS_ENABLE_WARNINGS=OFF)

message(STATUS "Using cxxopts")
