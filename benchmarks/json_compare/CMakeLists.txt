cmake_minimum_required(VERSION 3.16)
project(kitzoo_json_compare LANGUAGES CXX)

set(MJSON_SOURCE_DIR "" CACHE PATH "Path to mjson reconstructed sources")
set(NLOHMANN_INCLUDE_DIR "" CACHE PATH "Path containing nlohmann/json.hpp")
if(NOT EXISTS "${MJSON_SOURCE_DIR}/src/json.cpp" OR
   NOT EXISTS "${NLOHMANN_INCLUDE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "Set MJSON_SOURCE_DIR and NLOHMANN_INCLUDE_DIR to existing local sources")
endif()

add_executable(json_compare compare.cpp
    "${MJSON_SOURCE_DIR}/src/common.cpp"
    "${MJSON_SOURCE_DIR}/src/json.cpp"
    "${MJSON_SOURCE_DIR}/src/reader.cpp"
    "${MJSON_SOURCE_DIR}/src/key_spliter.cpp")
target_compile_features(json_compare PRIVATE cxx_std_20)
target_include_directories(json_compare PRIVATE
    "${MJSON_SOURCE_DIR}/include" "${NLOHMANN_INCLUDE_DIR}")
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR NOT UNIX)
    message(FATAL_ERROR "This benchmark currently requires GCC/Clang on Linux (optimization barrier)")
endif()
