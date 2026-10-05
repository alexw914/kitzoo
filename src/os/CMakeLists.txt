kitzoo_add_module(os SOURCES system.cpp fs.cpp DEPENDS core memory)
find_package(Threads REQUIRED)
target_link_libraries(kitzoo_os PUBLIC Threads::Threads)
