kitzoo_add_module(memory SOURCES basicmemory.cpp bytebuffer.cpp resource.cpp DEPENDS core)
find_package(Threads REQUIRED)
if(KITZOO_MIMALLOC_OVERRIDE)
    # The object file carries the whole allocator, so the static library stays
    # off the link line to avoid duplicate definitions. Link options precede the
    # executable's objects: mimalloc must initialize before any of them
    # allocates, as on macOS it takes over through the default malloc zone.
    add_library(kitzoo_mimalloc_init OBJECT mimalloc_init.cpp)
    target_include_directories(kitzoo_mimalloc_init PRIVATE "$<TARGET_PROPERTY:mimalloc-obj,INTERFACE_INCLUDE_DIRECTORIES>")
    set(KITZOO_MIMALLOC_OBJECTS "$<TARGET_OBJECTS:mimalloc-obj>" "$<TARGET_OBJECTS:kitzoo_mimalloc_init>")
    add_dependencies(kitzoo_memory mimalloc-obj kitzoo_mimalloc_init)
    target_include_directories(kitzoo_memory
        PUBLIC "$<BUILD_INTERFACE:$<TARGET_PROPERTY:mimalloc-obj,INTERFACE_INCLUDE_DIRECTORIES>>")
    target_link_libraries(kitzoo_memory PUBLIC Threads::Threads)
    target_link_options(kitzoo_memory INTERFACE "$<BUILD_INTERFACE:${KITZOO_MIMALLOC_OBJECTS}>")
    set_property(TARGET kitzoo_memory APPEND PROPERTY INTERFACE_LINK_DEPENDS
        "$<BUILD_INTERFACE:${KITZOO_MIMALLOC_OBJECTS}>")
    if(KITZOO_INSTALL)
        include(GNUInstallDirs)
        install(FILES "$<TARGET_OBJECTS:kitzoo_mimalloc_init>" DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                RENAME "kitzoo_mimalloc_init${CMAKE_CXX_OUTPUT_EXTENSION}")
    endif()
else()
    target_link_libraries(kitzoo_memory PUBLIC mimalloc-static Threads::Threads)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(kitzoo_memory PRIVATE rt)
endif()
