#include "app/Application.h"

#include <QApplication>
#include <QStyleFactory>
#include <QCoreApplication>

#include "app/StartupOptions.h"
#include "ui/MainWindow.h"
#include "core/Storage.h"

namespace qt_rsync::app {

int run(int argc, char** argv) {
  QApplication application(argc, argv);
  // qpy-rsync (PySide6) uses the Fusion widget style; match it so layouts and
  // control metrics are comparable when the two apps run side by side.
  if (QStyleFactory::keys().contains(QStringLiteral("Fusion"))) {
    QApplication::setStyle(QStringLiteral("Fusion"));
  }
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

  core::storage::migrateLegacyPaths();
  ui::MainWindow window;
  if (window.startupAborted()) return 0;
  window.applyStartupOptions(options);
  window.show();
  return QApplication::exec();
}

}  // namespace qt_rsync::app
