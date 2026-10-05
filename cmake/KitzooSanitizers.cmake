# ---------------------------------------------------------------------------
# Sanitizer configuration
# ---------------------------------------------------------------------------
# Parses KITZOO_SANITIZERS (semicolon-separated list) and computes:
#   KITZOO_SANITIZER_FLAGS       — compiler+linker flags (list)
#   KITZOO_ENABLED_SANITIZERS    — human-readable string for build_info
#
# Sanitizer flags are applied GLOBALLY via CMAKE_*_FLAGS because every
# translation unit — including FetchContent-built dependencies — must be
# instrumented for the runtime libraries to link correctly.
#
# kitzoo_apply_sanitizers(target)
#   Per-target hook that currently handles coverage instrumentation only.
#   Safe for INTERFACE (header-only) targets.
# ---------------------------------------------------------------------------

cmake_policy(SET CMP0057 NEW)

list(REMOVE_DUPLICATES KITZOO_SANITIZERS)

set(_valid_sanitizers address undefined thread memory)
set(_sanitizer_flags "")

foreach(_s IN LISTS KITZOO_SANITIZERS)
    if(NOT _s IN_LIST _valid_sanitizers)
        message(
            WARNING
                "Unknown sanitizer '${_s}'. Valid: address, undefined, thread, memory. Skipping."
        )
        continue()
    endif()

    # Thread sanitizer is incompatible with address and memory
    if(_s STREQUAL "thread")
        if("address" IN_LIST KITZOO_SANITIZERS OR "memory" IN_LIST KITZOO_SANITIZERS)
            message(
                FATAL_ERROR
                    "ThreadSanitizer is incompatible with Address/MemorySanitizer. "
                    "Remove one from KITZOO_SANITIZERS.")
        endif()
    endif()

    # Memory sanitizer is Linux+Clang only
    if(_s STREQUAL "memory")
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
            message(
                FATAL_ERROR
                    "MemorySanitizer requires Linux. Remove 'memory' from KITZOO_SANITIZERS.")
        endif()
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(
                FATAL_ERROR
                    "MemorySanitizer requires Clang. Remove 'memory' from KITZOO_SANITIZERS.")
        endif()
        list(APPEND _sanitizer_flags -fsanitize-memory-track-origins=2)
    endif()

    list(APPEND _sanitizer_flags -fsanitize=${_s})
endforeach()

# Address extra flags
if("address" IN_LIST KITZOO_SANITIZERS)
    list(APPEND _sanitizer_flags -fsanitize=pointer-compare -fsanitize=pointer-subtract)
endif()

# Common flags only when sanitizers are actually active
if(_sanitizer_flags)
    # UBSan: halt on error (fail fast in tests)
    if(NOT KITZOO_SANITIZE_RECOVER_ALL)
        list(APPEND _sanitizer_flags -fno-sanitize-recover=all)
    endif()
    # Better stack traces
    list(APPEND _sanitizer_flags -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()

# -- Apply sanitizer flags globally ----------------------------------------
if(_sanitizer_flags)
    foreach(_flag IN LISTS _sanitizer_flags)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_flag}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_flag}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_flag}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_flag}")
    endforeach()

    set(KITZOO_SANITIZER_FLAGS
        ${_sanitizer_flags}
        CACHE INTERNAL "Sanitizer flags applied globally")
endif()

# Human-readable string for build_info
list(JOIN KITZOO_SANITIZERS "," KITZOO_ENABLED_SANITIZERS)
if(NOT KITZOO_ENABLED_SANITIZERS)
    set(KITZOO_ENABLED_SANITIZERS "none")
endif()
set(KITZOO_ENABLED_SANITIZERS
    "${KITZOO_ENABLED_SANITIZERS}"
    CACHE INTERNAL "Sanitizers description string")

message(STATUS "Sanitizers: ${KITZOO_ENABLED_SANITIZERS}")

# -- kitzoo_apply_sanitizers -------------------------------------------------
# Sanitizer flags are global (above). This function only adds coverage
# instrumentation per target. Safe for INTERFACE (header-only) targets.
function(kitzoo_apply_sanitizers target)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        set(_scope INTERFACE)
    else()
        set(_scope PRIVATE)
    endif()

    if(KITZOO_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
            target_compile_options(
                ${target} ${_scope} -fprofile-instr-generate -fcoverage-mapping)
            target_link_options(${target} ${_scope} -fprofile-instr-generate)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # Atomic counters keep multithreaded tests from corrupting hit counts.
            target_compile_options(${target} ${_scope} --coverage -fprofile-update=atomic)
            target_link_options(${target} ${_scope} --coverage)
        endif()
    endif()
endfunction()

# Warn about benchmarks with sanitizers
if(KITZOO_BUILD_BENCHMARKS AND KITZOO_SANITIZER_FLAGS)
    message(
        STATUS
            "Sanitizers enabled with benchmarks — benchmark numbers will be unreliable "
            "(use sanitizer builds for correctness only).")
endif()