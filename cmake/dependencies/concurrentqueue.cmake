# ---------------------------------------------------------------------------
# moodycamel ConcurrentQueue — queue integration
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

kitzoo_fetch_dependency(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.5
    POPULATE_ONLY)

message(STATUS "Using moodycamel ConcurrentQueue")
