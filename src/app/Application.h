#pragma once

namespace qt_rsync::app {

// Creates the QApplication, parses CLI options, shows the main window and
// runs the event loop. Mirrors qpy_rsync.app.main() without any Python.
int run(int argc, char** argv);

}  // namespace qt_rsync::app
