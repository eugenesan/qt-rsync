find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(core_tests test_core.cpp)
target_link_libraries(core_tests PRIVATE Qt6::Test qt_rsync_core qt_rsync_model)
add_test(NAME core COMMAND core_tests)
set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
