find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(core_tests test_core.cpp)
target_link_libraries(core_tests PRIVATE Qt6::Test qt_rsync_core qt_rsync_model)
add_test(NAME core COMMAND core_tests)

qt_add_executable(ui_tests test_ui.cpp)
target_link_libraries(ui_tests PRIVATE Qt6::Test qt_rsync_ui qt_rsync_model qt_rsync_core)
add_test(NAME ui COMMAND ui_tests)
set_tests_properties(core ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
