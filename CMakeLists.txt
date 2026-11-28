cmake_minimum_required(VERSION 3.20)

project(qt-rsync
    VERSION 0.2.1
    DESCRIPTION "A pure Qt/C++ frontend for rsync"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)  # UIs are constructed in C++; see docs/decisions.md

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

option(QT_RSYNC_BUILD_TESTS "Build the QtTest suite" ON)

# Embed the checkout commit in --version so a shared binary can be identified.
set(QT_RSYNC_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE QT_RSYNC_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(QT_RSYNC_GIT_COMMIT STREQUAL "")
    set(QT_RSYNC_GIT_COMMIT "unknown")
  endif()
endif()

# ---------------------------------------------------------------------------
# Libraries. Keep one C++ source of truth (core) and let the UI observe it;
# there is no Python layer and no second decision model to keep in sync.
# ---------------------------------------------------------------------------
add_library(qt_rsync_core
    src/core/Change.cpp
    src/core/Decision.cpp
    src/core/Filter.cpp
    src/core/Profile.cpp
    src/core/Plan.cpp
    src/core/Command.cpp
    src/core/Paths.cpp
    src/core/Storage.cpp
    src/core/Parser.cpp
    src/core/ExternalTools.cpp
)
target_include_directories(qt_rsync_core PUBLIC src)
target_link_libraries(qt_rsync_core PUBLIC Qt6::Core)

add_library(qt_rsync_model
    src/model/DataStore.cpp
)
target_include_directories(qt_rsync_model PUBLIC src)
target_link_libraries(qt_rsync_model PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets qt_rsync_core)

add_library(qt_rsync_ui
    src/ui/MainWindow.cpp
    src/ui/ComparisonView.cpp
    src/ui/DecisionDelegate.cpp
    src/ui/OverviewPane.cpp
    src/ui/SettingsDialog.cpp
    src/ui/StatsBar.cpp
    src/ui/SyncDialog.cpp
    src/ui/UseAsLocationDialog.cpp
    src/ui/ProfileEditorDialog.cpp
    src/ui/LockedProfileDialog.cpp
    src/ui/Icons.cpp
    src/ui/FilterEditorWidget.cpp
    src/ui/TreeDelegate.cpp
    src/ui/ActionFilterBar.cpp
    src/ui/NativeDelegate.cpp
)
target_include_directories(qt_rsync_ui PUBLIC src)
target_link_libraries(qt_rsync_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets qt_rsync_model qt_rsync_core)

add_executable(qt-rsync
    src/main.cpp
    src/app/Application.cpp
    src/app/StartupOptions.cpp
    resources/resources.qrc
)
target_link_libraries(qt-rsync PRIVATE qt_rsync_ui qt_rsync_core Qt6::Widgets)
target_compile_definitions(qt-rsync PRIVATE
    QT_RSYNC_VERSION="${PROJECT_VERSION}"
    QT_RSYNC_GIT_COMMIT="${QT_RSYNC_GIT_COMMIT}")
set_target_properties(qt-rsync PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(GNUInstallDirs)
install(TARGETS qt-rsync RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES resources/qt-rsync.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES resources/qt-rsync.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

if(QT_RSYNC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
