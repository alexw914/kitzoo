configure_file(
    "${PROJECT_SOURCE_DIR}/include/kitzoo/core/version.hpp.in"
    "${PROJECT_BINARY_DIR}/include/kitzoo/core/version.hpp"
    @ONLY)

kitzoo_add_module(
    core
    SOURCES version.cpp)
