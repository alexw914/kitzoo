if(KITZOO_WITH_MIMALLOC)
    kitzoo_add_unit_test(mimalloc_allocator SOURCES mimalloc_allocator_test.cpp DEPENDS memory)
endif()
