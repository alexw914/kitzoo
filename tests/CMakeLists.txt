# ---------------------------------------------------------------------------
# Test infrastructure
# ---------------------------------------------------------------------------
# kitzoo_add_unit_test(name SOURCES ... [DEPENDS module ...] [TIMEOUT seconds])
#
# Creates a test executable linked against kitzoo::core and GTest::gtest_main,
# registered with CTest via gtest_discover_tests. Timeouts default to 30s.
# Sanitizer environment is configured per-test.
# ---------------------------------------------------------------------------

function(kitzoo_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    set(test_name kitzoo_test_${name})
    add_executable(${test_name} ${ARG_SOURCES})

    target_link_libraries(
        ${test_name}
        PRIVATE kitzoo::core
                GTest::gtest_main
                GTest::gmock)

    foreach(dep IN LISTS ARG_DEPENDS)
        target_link_libraries(${test_name} PRIVATE kitzoo::${dep})
    endforeach()

    target_compile_features(${test_name} PRIVATE cxx_std_20)
    set_target_properties(
        ${test_name}
        PROPERTIES CXX_STANDARD_REQUIRED ON
                   CXX_EXTENSIONS OFF)

    kitzoo_apply_warnings(${test_name})
    kitzoo_apply_sanitizers(${test_name})

    # Environment variables for sanitizers at test runtime
    set(_test_env "")
    list(APPEND _test_env "UBSAN_OPTIONS=print_stacktrace=1")
    list(APPEND _test_env "ASAN_OPTIONS=detect_container_overflow=1")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        list(APPEND _test_env "ASAN_OPTIONS=detect_leaks=0:detect_container_overflow=1")
    endif()
    list(APPEND _test_env "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1")

    string(JOIN ";" _sanitizer_env "${_test_env}")

    include(GoogleTest)
    gtest_discover_tests(
        ${test_name}
        EXTRA_ARGS --gtest_color=yes
        PROPERTIES ENVIRONMENT "${_sanitizer_env}")

    message(STATUS "Test suite: ${name}")
endfunction()

# -- Test subdirectories ------------------------------------------------------
add_subdirectory(core)
add_subdirectory(utilities)
add_subdirectory(lock)
add_subdirectory(string)
add_subdirectory(time)
add_subdirectory(thread)
add_subdirectory(cli)
add_subdirectory(queue)
add_subdirectory(memory)
add_subdirectory(log)
add_subdirectory(os)

if(KITZOO_WITH_OPENSSL)
    add_subdirectory(crypto)
endif()
add_subdirectory(json)
