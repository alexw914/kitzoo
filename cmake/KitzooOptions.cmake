# ---------------------------------------------------------------------------
# kitzoo build options
# ---------------------------------------------------------------------------

# Project-level guard: development targets are only built when kitzoo is the
# top-level project. As a subproject, they stay disabled even if a parent
# project has matching cache options enabled.

if(KITZOO_IS_TOP_LEVEL)
    option(KITZOO_BUILD_TESTS "Build test suite" ON)
    option(KITZOO_BUILD_BENCHMARKS "Build benchmarks" OFF)
    option(KITZOO_BUILD_EXAMPLES "Build examples" ON)
else()
    option(KITZOO_BUILD_TESTS "Build test suite" OFF)
    option(KITZOO_BUILD_BENCHMARKS "Build benchmarks" OFF)
    option(KITZOO_BUILD_EXAMPLES "Build examples" OFF)
    set(KITZOO_BUILD_TESTS OFF)
    set(KITZOO_BUILD_BENCHMARKS OFF)
    set(KITZOO_BUILD_EXAMPLES OFF)
endif()
option(KITZOO_BUILD_COMPARE_BENCHMARKS
       "Build comparison benchmarks against spdlog/BS::thread_pool"
       OFF)

option(KITZOO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(KITZOO_ENABLE_IPO "Enable interprocedural optimization (LTO)" OFF)
option(KITZOO_INSTALL "Generate install rules" ${KITZOO_IS_TOP_LEVEL})
option(KITZOO_ENABLE_COVERAGE "Build with code coverage instrumentation" OFF)

# -- Sanitizer configuration ---------------------------------------------------
# Semicolon-separated list: address, undefined, thread, memory
set(KITZOO_SANITIZERS ""
    CACHE STRING "Semicolon-separated sanitizers: address;undefined;thread;memory")
set_property(CACHE KITZOO_SANITIZERS PROPERTY STRINGS "address" "undefined" "thread" "memory" "address;undefined")

# When ON, UBSan reports halt the program immediately (fail fast in tests).
option(KITZOO_SANITIZE_RECOVER_ALL "Allow sanitizers to continue after first error" OFF)

# -- Third-party integrations --------------------------------------------------
# JSON/crypto are opt-in. ConcurrentQueue and BS::thread_pool are always fetched.
option(KITZOO_WITH_OPENSSL "Enable crypto module (requires OpenSSL)" OFF)
option(KITZOO_WITH_MIMALLOC "Enable the mimalloc allocator adapter" OFF)

# -- Dependency provider options -----------------------------------------------
option(KITZOO_USE_SYSTEM_GOOGLETEST "Use system-installed GoogleTest" OFF)
option(KITZOO_USE_SYSTEM_BENCHMARK "Use system-installed Google Benchmark" OFF)
