cmake_minimum_required(VERSION 3.20)

project(qt-rsync
    VERSION 0.1.0
    DESCRIPTION "A pure Qt/C++ frontend for rsync"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)  # UIs are constructed in C++; see docs/DECISIONS.md

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

option(QT_RSYNC_BUILD_TESTS "Build the QtTest suite" ON)

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
    src/ui/LockedProfileDialog.cpp
)
target_include_directories(qt_rsync_ui PUBLIC src)
target_link_libraries(qt_rsync_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets qt_rsync_model qt_rsync_core)

add_executable(qt-rsync
    src/main.cpp
    src/app/Application.cpp
    src/app/StartupOptions.cpp
)
target_link_libraries(qt-rsync PRIVATE qt_rsync_ui qt_rsync_core Qt6::Widgets)
set_target_properties(qt-rsync PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(QT_RSYNC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
