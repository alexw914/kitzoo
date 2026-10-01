# ---------------------------------------------------------------------------
# GoogleTest — unit-test framework (fetched when KITZOO_BUILD_TESTS=ON)
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

if(KITZOO_USE_SYSTEM_GOOGLETEST)
    find_package(GTest CONFIG REQUIRED)
    message(STATUS "Using system GoogleTest")
    return()
endif()

kitzoo_fetch_dependency(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    OPTIONS INSTALL_GTEST=OFF BUILD_GMOCK=ON
    FIND_PACKAGE NAMES GTest)

message(STATUS "Using GoogleTest")
