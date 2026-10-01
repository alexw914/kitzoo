# ---------------------------------------------------------------------------
# BS::thread_pool — thread-pool integration and comparison benchmark
#
# Upstream repo has no root CMakeLists.txt, so sources are fetched without
# add_subdirectory and the interface target is defined here.
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

kitzoo_fetch_dependency(
    thread_pool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
    GIT_TAG v5.1.0
    POPULATE_ONLY)

if(NOT TARGET BS::thread_pool)
    add_library(bs_thread_pool INTERFACE)
    add_library(BS::thread_pool ALIAS bs_thread_pool)
    target_include_directories(bs_thread_pool SYSTEM INTERFACE "${thread_pool_SOURCE_DIR}/include")
endif()

message(STATUS "Using BS::thread_pool")
