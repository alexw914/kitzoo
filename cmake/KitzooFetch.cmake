# ---------------------------------------------------------------------------
# kitzoo dependency fetching
# ---------------------------------------------------------------------------
# kitzoo_fetch_dependency(<name>
#     GIT_REPOSITORY <url>     # default upstream repository
#     GIT_TAG <tag>            # default tag/commit
#     [OPTIONS key=value ...]  # cache variables FORCED before fetching
#     [FIND_PACKAGE args ...]  # forwarded as FIND_PACKAGE_ARGS (git mode only):
#                              # try find_package before downloading
#     [POPULATE_ONLY]          # fetch sources without add_subdirectory; sets
#                              # <name>_SOURCE_DIR for the caller to consume
# )
#
# Source selection, first match wins:
#   1. KITZOO_USE_LOCAL_3RDPARTY=ON → require ${KITZOO_3RDPARTY_DIR}/<name>
#   2. KITZOO_DEP_<NAME>_FILE set   → explicit source archive (URL or path)
#   3. local source exists          → ${KITZOO_3RDPARTY_DIR}/<name>
#   4. default                      → git clone; URL/tag overridable via
#      KITZOO_DEP_<NAME>_GIT_URL / KITZOO_DEP_<NAME>_GIT_TAG (mirror support)
#
# <NAME> is the dependency name upper-cased (nlohmann_json → NLOHMANN_JSON).
# ---------------------------------------------------------------------------

include(FetchContent)

set(KITZOO_FETCHCONTENT_BASE_DIR "${PROJECT_BINARY_DIR}/_deps"
    CACHE PATH "FetchContent download directory")

set(KITZOO_3RDPARTY_DIR "${PROJECT_SOURCE_DIR}/3rdparty"
    CACHE PATH "Directory with local third-party source checkouts")
option(KITZOO_USE_LOCAL_3RDPARTY
       "Require vendored sources from KITZOO_3RDPARTY_DIR (no download fallback)" OFF)

function(kitzoo_fetch_dependency name)
    cmake_parse_arguments(ARG "POPULATE_ONLY" "GIT_REPOSITORY;GIT_TAG"
                          "OPTIONS;FIND_PACKAGE" ${ARGN})

    if(NOT ARG_GIT_REPOSITORY OR NOT ARG_GIT_TAG)
        message(FATAL_ERROR
            "kitzoo_fetch_dependency(${name}): GIT_REPOSITORY and GIT_TAG are required")
    endif()

    string(TOUPPER "${name}" upper)

    # Overridable per-dependency source coordinates.
    set(KITZOO_DEP_${upper}_GIT_URL "${ARG_GIT_REPOSITORY}"
        CACHE STRING "${name} git repository (override to use a mirror)")
    set(KITZOO_DEP_${upper}_GIT_TAG "${ARG_GIT_TAG}"
        CACHE STRING "${name} git tag/commit")
    set(KITZOO_DEP_${upper}_FILE ""
        CACHE STRING "${name} source archive (URL or local path); overrides git")

    foreach(opt IN LISTS ARG_OPTIONS)
        if(NOT opt MATCHES "^([A-Za-z_0-9]+)=(.*)$")
            message(FATAL_ERROR
                "kitzoo_fetch_dependency(${name}): bad OPTION '${opt}' (expected key=value)")
        endif()
        set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE BOOL "" FORCE)
    endforeach()

    set(local_dir "${KITZOO_3RDPARTY_DIR}/${name}")
    set(populate_only_args)
    if(ARG_POPULATE_ONLY)
        # FetchContent_MakeAvailable() populates the source but skips
        # add_subdirectory() because this deliberately has no CMakeLists.txt.
        set(populate_only_args SOURCE_SUBDIR _kitzoo_fetch_only)
    endif()

    set(local_source_exists FALSE)
    if(EXISTS "${local_dir}" AND (ARG_POPULATE_ONLY OR EXISTS "${local_dir}/CMakeLists.txt"))
        set(local_source_exists TRUE)
    endif()

    if(KITZOO_USE_LOCAL_3RDPARTY)
        if(NOT local_source_exists)
            message(FATAL_ERROR
                "KITZOO_USE_LOCAL_3RDPARTY is ON but ${name} was not found at "
                "${local_dir}")
        endif()
        message(STATUS "Dependency ${name}: local ${local_dir}")
        FetchContent_Declare(${name} SOURCE_DIR "${local_dir}" ${populate_only_args})
    elseif(KITZOO_DEP_${upper}_FILE)
        message(STATUS "Dependency ${name}: archive ${KITZOO_DEP_${upper}_FILE}")
        FetchContent_Declare(${name} URL "${KITZOO_DEP_${upper}_FILE}" ${populate_only_args})
    elseif(local_source_exists)
        message(STATUS "Dependency ${name}: local ${local_dir}")
        FetchContent_Declare(${name} SOURCE_DIR "${local_dir}" ${populate_only_args})
    else()
        message(STATUS
            "Dependency ${name}: git ${KITZOO_DEP_${upper}_GIT_URL} @ ${KITZOO_DEP_${upper}_GIT_TAG}")
        if(ARG_FIND_PACKAGE)
            set(find_package_args FIND_PACKAGE_ARGS ${ARG_FIND_PACKAGE})
        endif()
        FetchContent_Declare(
            ${name}
            GIT_REPOSITORY "${KITZOO_DEP_${upper}_GIT_URL}"
            GIT_TAG "${KITZOO_DEP_${upper}_GIT_TAG}"
            GIT_SHALLOW TRUE
            # Integrations use upstream sources only; no submodules are needed.
            GIT_SUBMODULES ""
            GIT_PROGRESS TRUE
            ${populate_only_args}
            ${find_package_args})
    endif()

    if(ARG_POPULATE_ONLY)
        FetchContent_MakeAvailable(${name})
        set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
        return()
    endif()

    FetchContent_MakeAvailable(${name})
endfunction()
