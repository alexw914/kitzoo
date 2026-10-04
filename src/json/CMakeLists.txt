add_library(kitzoo_json INTERFACE)
add_library(kitzoo::json ALIAS kitzoo_json)
set_target_properties(kitzoo_json PROPERTIES EXPORT_NAME json)
target_include_directories(kitzoo_json INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(kitzoo_json INTERFACE cxx_std_20)
target_link_libraries(
    kitzoo_json INTERFACE kitzoo::os nlohmann_json::nlohmann_json)
kitzoo_apply_warnings(kitzoo_json)
kitzoo_apply_sanitizers(kitzoo_json)
if(KITZOO_INSTALL)
    install(TARGETS kitzoo_json EXPORT kitzooTargets)
endif()
