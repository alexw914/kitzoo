# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
# kitzoo_add_example(name SOURCES ... [DEPENDS module ...])
#
# Creates an example executable kitzoo_example_<name> linked against the
# given kitzoo modules.
# ---------------------------------------------------------------------------

function(kitzoo_add_example name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(kitzoo_example_${name} ${ARG_SOURCES})

    foreach(dep IN LISTS ARG_DEPENDS)
        target_link_libraries(kitzoo_example_${name} PRIVATE kitzoo::${dep})
    endforeach()

    target_compile_features(kitzoo_example_${name} PRIVATE cxx_std_20)
    set_target_properties(
        kitzoo_example_${name}
        PROPERTIES CXX_STANDARD_REQUIRED ON
                   CXX_EXTENSIONS OFF)
    kitzoo_apply_warnings(kitzoo_example_${name})
endfunction()

kitzoo_add_example(strings SOURCES strings.cpp DEPENDS string)
kitzoo_add_example(time SOURCES time.cpp DEPENDS time)
kitzoo_add_example(json SOURCES json.cpp DEPENDS json)
kitzoo_add_example(cli SOURCES cli.cpp DEPENDS cli)
kitzoo_add_example(logging SOURCES logging.cpp DEPENDS log)
kitzoo_add_example(queues SOURCES queues.cpp DEPENDS queue)
kitzoo_add_example(memory SOURCES memory.cpp DEPENDS thread thread_concurrent_pool)
kitzoo_add_example(filesystem SOURCES filesystem.cpp DEPENDS filesystem)
kitzoo_add_example(utilities SOURCES utilities.cpp DEPENDS utilities system)

kitzoo_add_example(thread SOURCES thread.cpp)
target_link_libraries(kitzoo_example_thread
                      PRIVATE kitzoo::queue_concurrentqueue kitzoo::thread_bs_pool)

if(KITZOO_WITH_OPENSSL)
    kitzoo_add_example(crypto SOURCES crypto.cpp DEPENDS crypto)
endif()
