add_library(kitzoo_cli INTERFACE)
add_library(kitzoo::cli ALIAS kitzoo_cli)
set_target_properties(kitzoo_cli PROPERTIES EXPORT_NAME cli)
target_include_directories(
    kitzoo_cli INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(kitzoo_cli INTERFACE cxx_std_20)
target_link_libraries(kitzoo_cli INTERFACE cxxopts::cxxopts)
kitzoo_apply_warnings(kitzoo_cli)
kitzoo_apply_sanitizers(kitzoo_cli)

if(KITZOO_INSTALL)
    install(TARGETS kitzoo_cli EXPORT kitzooTargets)
endif()
