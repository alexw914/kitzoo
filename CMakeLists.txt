cmake_minimum_required(VERSION 3.25)

project(
    kitzoo
    VERSION 0.2.0
    DESCRIPTION "A cross-platform C++20 foundation library"
    LANGUAGES CXX)

# -- Project detection ---------------------------------------------------------
set(KITZOO_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(KITZOO_IS_TOP_LEVEL ON)
endif()

# -- Module path ---------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# -- CMake modules ------------------------------------------------------------
include(KitzooOptions)
include(KitzooWarnings)
include(KitzooSanitizers)
include(KitzooDependencies)
include(KitzooModule)
include(KitzooInstall)

# -- Global C++ standard settings ---------------------------------------------
# These apply to all targets in the project and propagate via target properties.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Deduplicate static libraries on link lines; Apple ld warns on repeated archives.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    set(CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY)
endif()

if(KITZOO_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(KITZOO_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT output)
    if(supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(STATUS "IPO not supported: ${output}")
    endif()
endif()

# -- Ccache / sccache ----------------------------------------------------------
find_program(CCACHE_PROGRAM ccache sccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using compiler cache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# -- Subdirectories -----------------------------------------------------------
# Generate build_config.hpp for version.cpp (needs sanitizer info from above)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_config.hpp.in"
    "${CMAKE_BINARY_DIR}/include/kitzoo/core/build_config.hpp"
    @ONLY)

# Core module (build system validation target — contains only version/build info)
add_subdirectory(src)

# Tests
if(KITZOO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(KITZOO_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
if(KITZOO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
