# ---------------------------------------------------------------------------
# kitzoo module creation
# ---------------------------------------------------------------------------
# kitzoo_add_module(name [SOURCES src1.cpp ...] [DEPENDS module1 ...])
#
# Creates target kitzoo_<name> with alias kitzoo::<name>.
# - With SOURCES:    static library (compiled module).
# - Without SOURCES: INTERFACE library (header-only module).
#
# - Sets C++20 standard (required, no extensions).
# - Adds source and generated include directories.
# - Applies warnings and sanitizer flags; compiled modules also get PIC and
#   hidden visibility.
# - Installs headers and target in the kitzoo export set.
#
# Usage example (in src/core/CMakeLists.txt):
#   kitzoo_add_module(core SOURCES version.cpp)
#   kitzoo_add_module(memory DEPENDS core)    # header-only
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

function(kitzoo_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    set(target_name kitzoo_${name})

    if(ARG_SOURCES)
        add_library(${target_name} STATIC)
        target_sources(${target_name} PRIVATE ${ARG_SOURCES})
        set(scope PUBLIC)
    else()
        add_library(${target_name} INTERFACE)
        set(scope INTERFACE)
    endif()
    add_library(kitzoo::${name} ALIAS ${target_name})

    target_include_directories(
        ${target_name}
        ${scope} $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                 $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    target_compile_features(${target_name} ${scope} cxx_std_20)

    if(ARG_SOURCES)
        set_target_properties(
            ${target_name}
            PROPERTIES CXX_STANDARD_REQUIRED ON
                       CXX_EXTENSIONS OFF
                       POSITION_INDEPENDENT_CODE ON
                       CXX_VISIBILITY_PRESET hidden
                       VISIBILITY_INLINES_HIDDEN ON)
    endif()
    set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${name})

    kitzoo_apply_warnings(${target_name})
    kitzoo_apply_sanitizers(${target_name})

    foreach(dep IN LISTS ARG_DEPENDS)
        if(NOT TARGET kitzoo::${dep})
            message(
                FATAL_ERROR
                    "kitzoo_add_module(${name}): dependency '${dep}' not found. "
                    "Ensure it is added before this module.")
        endif()
        target_link_libraries(${target_name} ${scope} kitzoo::${dep})
    endforeach()

    # Install rules
    if(KITZOO_INSTALL)
        if(ARG_SOURCES)
            install(
                TARGETS ${target_name}
                EXPORT kitzooTargets
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        else()
            install(TARGETS ${target_name} EXPORT kitzooTargets)
        endif()
    endif()
endfunction()