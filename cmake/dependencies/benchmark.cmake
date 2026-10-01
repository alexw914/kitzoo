# ---------------------------------------------------------------------------
# Google Benchmark — microbenchmark framework (KITZOO_BUILD_BENCHMARKS=ON)
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

if(KITZOO_USE_SYSTEM_BENCHMARK)
    find_package(benchmark CONFIG REQUIRED)
    message(STATUS "Using system Google Benchmark")
    return()
endif()

kitzoo_fetch_dependency(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
    OPTIONS BENCHMARK_ENABLE_TESTING=OFF BENCHMARK_ENABLE_INSTALL=OFF
    FIND_PACKAGE NAMES benchmark)

message(STATUS "Using Google Benchmark")
