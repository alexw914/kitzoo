kitzoo_add_benchmark(thread_pool SOURCES thread_pool_bench.cpp DEPENDS thread)

if(KITZOO_BUILD_COMPARE_BENCHMARKS)
    include(dependencies/thread_pool)
    add_executable(kitzoo_compare_threadpool compare_threadpool.cpp)
    target_compile_features(kitzoo_compare_threadpool PRIVATE cxx_std_20)
    target_link_libraries(
        kitzoo_compare_threadpool
        PRIVATE kitzoo::thread BS::thread_pool benchmark::benchmark benchmark::benchmark_main)
endif()
