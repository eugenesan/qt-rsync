#include "app/Application.h"

#include <QApplication>
#include <QProxyStyle>
#include <QStyleHintReturn>
#include <QCoreApplication>
#include <QIcon>

#include "app/StartupOptions.h"
#include "ui/MainWindow.h"
#include "core/Storage.h"

namespace qt_rsync::app {
namespace {

class TooltipStyle : public QProxyStyle {
 public:
  using QProxyStyle::QProxyStyle;

  int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                const QWidget* widget = nullptr,
                QStyleHintReturn* returnData = nullptr) const override {
    if (hint == QStyle::SH_ToolTip_WakeUpDelay) return 250;
    if (hint == QStyle::SH_ToolTip_FallAsleepDelay) return 0;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};

}  // namespace

void installTooltipStyle(QApplication& app) {
  app.setStyle(new TooltipStyle(app.style()));
}

int run(int argc, char** argv) {
  QApplication application(argc, argv);
  installTooltipStyle(application);
  // Do not force a widget style. qpy-rsync inherits the platform theme, so
  // under KDE both apps use Breeze and the same dark palette. Forcing Fusion
  // here produced the wrong colours, separators and button frames.
  QCoreApplication::setApplicationName(QStringLiteral("qt-rsync"));
#ifndef QT_RSYNC_VERSION
#define QT_RSYNC_VERSION "0.2.1"
#endif
#ifndef QT_RSYNC_GIT_COMMIT
#define QT_RSYNC_GIT_COMMIT "unknown"
#endif
  QString version = QStringLiteral(QT_RSYNC_VERSION);
  const QString commit = QStringLiteral(QT_RSYNC_GIT_COMMIT);
  if (!commit.isEmpty() && commit != QLatin1String("unknown")) {
    version += QStringLiteral(" (%1)").arg(commit);
  }
  QCoreApplication::setApplicationVersion(version);
  QCoreApplication::setOrganizationName(QStringLiteral("qt-rsync"));
  application.setWindowIcon(QIcon(QStringLiteral(":/qt-rsync.svg")));

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

  core::storage::removeObsoleteEncodedSettings();
  ui::MainWindow window;
  if (window.startupAborted()) return 0;
  window.applyStartupOptions(options);
  window.show();
  return QApplication::exec();
}

}  // namespace qt_rsync::app
