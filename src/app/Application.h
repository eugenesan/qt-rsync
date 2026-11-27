#pragma once

class QApplication;

namespace qt_rsync::app {

// Applies a uniform 250 ms tooltip wake-up delay and no fall-asleep grace,
// so moving between widgets does not show the next tooltip instantly.
void installTooltipStyle(QApplication& app);

// Creates the QApplication, parses CLI options, shows the main window and
// runs the event loop. Mirrors qpy_rsync.app.main() without any Python.
int run(int argc, char** argv);

}  // namespace qt_rsync::app
