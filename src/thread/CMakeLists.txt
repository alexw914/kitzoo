kitzoo_add_module(thread SOURCES thread_pool.cpp DEPENDS core utilities)

add_library(kitzoo_thread_concurrent_pool INTERFACE)
add_library(kitzoo::thread_concurrent_pool ALIAS kitzoo_thread_concurrent_pool)
set_target_properties(kitzoo_thread_concurrent_pool PROPERTIES EXPORT_NAME thread_concurrent_pool)
target_link_libraries(kitzoo_thread_concurrent_pool
                      INTERFACE kitzoo::thread kitzoo::queue_concurrentqueue)
if(KITZOO_INSTALL)
    install(TARGETS kitzoo_thread_concurrent_pool EXPORT kitzooTargets)
endif()

include(GNUInstallDirs)

add_library(kitzoo_thread_bs_pool INTERFACE)
add_library(kitzoo::thread_bs_pool ALIAS kitzoo_thread_bs_pool)
set_target_properties(kitzoo_thread_bs_pool PROPERTIES EXPORT_NAME thread_bs_pool)
target_include_directories(
    kitzoo_thread_bs_pool SYSTEM
    INTERFACE $<BUILD_INTERFACE:${thread_pool_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/kitzoo/third_party/bs_thread_pool>)

if(KITZOO_INSTALL)
    install(TARGETS kitzoo_thread_bs_pool EXPORT kitzooTargets)
    install(FILES
        "${thread_pool_SOURCE_DIR}/include/BS_thread_pool.hpp"
        "${thread_pool_SOURCE_DIR}/LICENSE.txt"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kitzoo/third_party/bs_thread_pool")
endif()
