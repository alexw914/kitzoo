kitzoo_add_unit_test(log SOURCES log_test.cpp DEPENDS log)
kitzoo_add_unit_test(file_sink SOURCES file_sink_test.cpp DEPENDS log os)
