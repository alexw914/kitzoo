# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------

if(NOT KITZOO_INSTALL)
    return()
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Source-tree headers
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT devel
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
    PATTERN "*.hpp.in" EXCLUDE)

# Generated headers (version.hpp from configure_file, etc.)
install(
    DIRECTORY "${PROJECT_BINARY_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT devel
    FILES_MATCHING
    PATTERN "*.hpp")

# Package config
set(KITZOO_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/kitzoo")

# Consumers need transitive dependencies of compiled modules.
set(KITZOO_CONFIG_FIND_DEPS "")
string(APPEND KITZOO_CONFIG_FIND_DEPS "find_dependency(Threads)\n")
string(APPEND KITZOO_CONFIG_FIND_DEPS "find_dependency(spdlog CONFIG)")
string(APPEND KITZOO_CONFIG_FIND_DEPS "\nfind_dependency(nlohmann_json CONFIG)")
string(APPEND KITZOO_CONFIG_FIND_DEPS "\nfind_dependency(cxxopts CONFIG)")
if(KITZOO_WITH_OPENSSL)
    string(APPEND KITZOO_CONFIG_FIND_DEPS "\nfind_dependency(OpenSSL)")
endif()
if(KITZOO_WITH_MIMALLOC)
    string(APPEND KITZOO_CONFIG_FIND_DEPS "\nfind_dependency(mimalloc CONFIG)")
endif()

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/kitzooConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/kitzooConfig.cmake"
    INSTALL_DESTINATION "${KITZOO_INSTALL_CONFIGDIR}"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/kitzooConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

install(
    FILES "${PROJECT_BINARY_DIR}/kitzooConfig.cmake"
          "${PROJECT_BINARY_DIR}/kitzooConfigVersion.cmake"
    DESTINATION "${KITZOO_INSTALL_CONFIGDIR}")

install(
    EXPORT kitzooTargets
    NAMESPACE kitzoo::
    FILE kitzooTargets.cmake
    DESTINATION "${KITZOO_INSTALL_CONFIGDIR}")
