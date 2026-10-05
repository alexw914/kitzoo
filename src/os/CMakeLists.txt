kitzoo_add_module(os SOURCES osadaptor.cpp fsadaptor.cpp DEPENDS core utilities)
find_package(Threads REQUIRED)
target_link_libraries(kitzoo_os PUBLIC Threads::Threads)
