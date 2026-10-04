kitzoo_add_unit_test(time
    SOURCES time_test.cpp timeline_test.cpp time_watcher_test.cpp
    DEPENDS time)
kitzoo_add_unit_test(timer SOURCES timer_test.cpp DEPENDS time)
