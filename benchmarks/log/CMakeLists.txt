kitzoo_add_benchmark(log SOURCES log_bench.cpp DEPENDS log)

if(KITZOO_BUILD_COMPARE_BENCHMARKS)
    add_executable(kitzoo_compare_log compare_log.cpp)
    target_compile_features(kitzoo_compare_log PRIVATE cxx_std_20)
    target_link_libraries(
        kitzoo_compare_log PRIVATE kitzoo::log benchmark::benchmark benchmark::benchmark_main)
endif()
