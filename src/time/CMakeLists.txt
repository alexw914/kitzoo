kitzoo_add_module(time
    SOURCES time.cpp timeline.cpp time_watcher.cpp timer.cpp
    DEPENDS core memory)
find_package(Threads REQUIRED)
target_link_libraries(kitzoo_time PUBLIC Threads::Threads)
