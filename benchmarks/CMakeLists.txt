# ---------------------------------------------------------------------------
# Benchmark infrastructure
# ---------------------------------------------------------------------------
# kitzoo_add_benchmark(name SOURCES ... [DEPENDS module ...])
#
# Creates a benchmark executable linked against kitzoo::core and
# benchmark::benchmark_main, then registers it as a CTest test with a short
# minimum time for smoke validation.
# ---------------------------------------------------------------------------

function(kitzoo_add_benchmark name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    set(bench_name kitzoo_bench_${name})
    add_executable(${bench_name} ${ARG_SOURCES})

    target_link_libraries(
        ${bench_name}
        PRIVATE kitzoo::core
                benchmark::benchmark
                benchmark::benchmark_main)

    foreach(dep IN LISTS ARG_DEPENDS)
        target_link_libraries(${bench_name} PRIVATE kitzoo::${dep})
    endforeach()

    target_compile_features(${bench_name} PRIVATE cxx_std_20)
    set_target_properties(
        ${bench_name}
        PROPERTIES CXX_STANDARD_REQUIRED ON
                   CXX_EXTENSIONS OFF)

    kitzoo_apply_warnings(${bench_name})
    # Do NOT apply sanitizers to benchmarks — they invalidate measurements

    add_test(
        NAME bench_${name}
        COMMAND ${bench_name} --benchmark_min_time=0.01s
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")

    set_tests_properties(bench_${name} PROPERTIES LABELS benchmark)

    message(STATUS "Benchmark suite: ${name}")
endfunction()

# -- Benchmark subdirectories -------------------------------------------------
add_subdirectory(queue)
add_subdirectory(thread)
add_subdirectory(log)
