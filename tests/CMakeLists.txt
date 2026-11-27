find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(core_tests test_core.cpp)
target_link_libraries(core_tests PRIVATE Qt6::Test qt_rsync_core qt_rsync_model)
add_test(NAME core COMMAND core_tests)

qt_add_executable(ui_tests test_ui.cpp)
target_link_libraries(ui_tests PRIVATE Qt6::Test qt_rsync_ui qt_rsync_model qt_rsync_core)
target_compile_definitions(ui_tests PRIVATE QT_RSYNC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_sources(ui_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/resources/resources.qrc"
    "${CMAKE_SOURCE_DIR}/src/app/StartupOptions.cpp"
    "${CMAKE_SOURCE_DIR}/src/app/Application.cpp")
add_test(NAME ui COMMAND ui_tests)
add_test(NAME install COMMAND "${CMAKE_COMMAND}"
         "-DQT_RSYNC_BINARY_DIR=${CMAKE_BINARY_DIR}"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/check_install.cmake")
set_tests_properties(core ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
