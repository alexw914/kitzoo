# ---------------------------------------------------------------------------
# nlohmann/json — JSON module dependency
# ---------------------------------------------------------------------------

include_guard(GLOBAL)
include(KitzooFetch)

if(TARGET nlohmann_json::nlohmann_json)
    message(STATUS "Using nlohmann/json (existing target)")
    return()
endif()

if(KITZOO_INSTALL)
    set(_json_install ON)
else()
    set(_json_install OFF)
endif()

kitzoo_fetch_dependency(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    OPTIONS JSON_BuildTests=OFF JSON_Install=${_json_install})

message(STATUS "Using nlohmann/json")
