kitzoo_add_module(log SOURCES log.cpp file_sink.cpp DEPENDS core time queue)
target_link_libraries(kitzoo_log PUBLIC
    $<BUILD_INTERFACE:spdlog::spdlog>
    $<INSTALL_INTERFACE:spdlog::spdlog>)

target_link_libraries(kitzoo_log PUBLIC kitzoo::memory)
