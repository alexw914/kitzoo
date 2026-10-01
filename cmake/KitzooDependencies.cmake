# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------
# Thin orchestrator. Each dependency lives in its own module under
# cmake/dependencies/ and is fetched via kitzoo_fetch_dependency()
# (see KitzooFetch.cmake): local source checkout → source archive → git, with
# mirror-overridable URLs/tags.
#
# Adding a new dependency: create cmake/dependencies/<name>.cmake and
# include it here, with optional dependencies gated by their build option.
# ---------------------------------------------------------------------------

include(KitzooFetch)

if(KITZOO_BUILD_TESTS)
    include(dependencies/googletest)
endif()

if(KITZOO_BUILD_BENCHMARKS)
    include(dependencies/benchmark)
endif()

# The log module uses spdlog as its backend.
include(dependencies/spdlog)

include(dependencies/nlohmann_json)

include(dependencies/concurrentqueue)
include(dependencies/thread_pool)
include(dependencies/cxxopts)

if(KITZOO_WITH_MIMALLOC)
    include(dependencies/mimalloc)
endif()
