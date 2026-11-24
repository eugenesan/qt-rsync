# Verify that a staged install contains the binary, desktop entry and icon.
if(NOT DEFINED QT_RSYNC_BINARY_DIR)
  message(FATAL_ERROR "QT_RSYNC_BINARY_DIR is required")
endif()
set(stage "${CMAKE_CURRENT_BINARY_DIR}/install-stage")
file(REMOVE_RECURSE "${stage}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${QT_RSYNC_BINARY_DIR}" --prefix "${stage}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE output)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cmake --install failed:\n${output}")
endif()
foreach(path
    "bin/qt-rsync"
    "share/applications/qt-rsync.desktop"
    "share/icons/hicolor/scalable/apps/qt-rsync.svg")
  if(NOT EXISTS "${stage}/${path}")
    message(FATAL_ERROR "missing installed file: ${path}")
  endif()
endforeach()
file(READ "${stage}/share/applications/qt-rsync.desktop" desktop)
foreach(field "Name=qt-rsync" "Exec=qt-rsync" "Icon=qt-rsync")
  string(FIND "${desktop}" "${field}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "desktop entry is missing ${field}")
  endif()
endforeach()
message(STATUS "installed files and desktop entry verified")
