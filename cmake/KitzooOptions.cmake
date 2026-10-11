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
# AES utilities are opt-in. Other module dependencies are always fetched.
option(KITZOO_WITH_OPENSSL "Enable OpenSSL AES utilities" OFF)

# Executables linking kitzoo::memory get mimalloc's single object file, which
# replaces malloc/free and new/delete for the whole process. Windows needs the
# static CRT for this and sanitizers bring their own allocator, so both opt out.
option(KITZOO_MIMALLOC_OVERRIDE "Replace the process allocator with mimalloc" ON)
if(KITZOO_MIMALLOC_OVERRIDE AND (WIN32 OR KITZOO_SANITIZERS))
    message(STATUS "kitzoo: mimalloc override disabled on Windows and in sanitizer builds")
    set(KITZOO_MIMALLOC_OVERRIDE OFF CACHE BOOL "Replace the process allocator with mimalloc" FORCE)
endif()

# -- Dependency provider options -----------------------------------------------
option(KITZOO_USE_SYSTEM_GOOGLETEST "Use system-installed GoogleTest" OFF)
option(KITZOO_USE_SYSTEM_BENCHMARK "Use system-installed Google Benchmark" OFF)
