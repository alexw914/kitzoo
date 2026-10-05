include(GNUInstallDirs)

kitzoo_add_module(queue DEPENDS core memory)

target_include_directories(
    kitzoo_queue SYSTEM
    INTERFACE $<BUILD_INTERFACE:${concurrentqueue_SOURCE_DIR}>
              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/kitzoo/third_party/concurrentqueue>)

add_library(kitzoo_queue_concurrentqueue INTERFACE)
add_library(kitzoo::queue_concurrentqueue ALIAS kitzoo_queue_concurrentqueue)
set_target_properties(kitzoo_queue_concurrentqueue PROPERTIES EXPORT_NAME queue_concurrentqueue)
target_link_libraries(kitzoo_queue_concurrentqueue INTERFACE kitzoo::queue)
if(KITZOO_INSTALL)
    install(TARGETS kitzoo_queue_concurrentqueue EXPORT kitzooTargets)
    install(FILES
        "${concurrentqueue_SOURCE_DIR}/concurrentqueue.h"
        "${concurrentqueue_SOURCE_DIR}/blockingconcurrentqueue.h"
        "${concurrentqueue_SOURCE_DIR}/lightweightsemaphore.h"
        "${concurrentqueue_SOURCE_DIR}/LICENSE.md"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kitzoo/third_party/concurrentqueue")
endif()
