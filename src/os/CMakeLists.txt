kitzoo_add_module(os SOURCES sys.cpp filesys.cpp DEPENDS core memory)
find_package(Threads REQUIRED)
target_link_libraries(kitzoo_os PUBLIC Threads::Threads)
