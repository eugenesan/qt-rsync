#include "app/Application.h"

#include <QApplication>
#include <QCoreApplication>

#include "app/StartupOptions.h"
#include "ui/MainWindow.h"

namespace qt_rsync::app {

int run(int argc, char** argv) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("qt-rsync"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QCoreApplication::setOrganizationName(QStringLiteral("qt-rsync"));

  QString error;
  const StartupOptions options = StartupOptions::parse(
      QCoreApplication::arguments(), &error);
  if (!error.isEmpty()) return 2;
  if (options.help) {
    StartupOptions::printHelp();
    return 0;
  }
  if (options.version) {
    StartupOptions::printVersion();
    return 0;
  }

  ui::MainWindow window;
  window.applyStartupOptions(options);
  window.show();
  return QApplication::exec();
}

}  // namespace qt_rsync::app
