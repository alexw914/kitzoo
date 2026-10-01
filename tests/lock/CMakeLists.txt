kitzoo_add_unit_test(spinlock SOURCES spinlock_test.cpp DEPENDS lock)
kitzoo_add_unit_test(rw_spinlock SOURCES rw_spinlock_test.cpp DEPENDS lock)
kitzoo_add_unit_test(synchronized SOURCES synchronized_test.cpp DEPENDS lock)
