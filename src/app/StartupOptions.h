#pragma once

#include <QCommandLineParser>
#include <QString>

namespace qt_rsync::app {

// Same options as the qpy-rsync GUI launcher, parsed with QCommandLineParser.
// There is no separate CLI binary; --version just prints and exits.
struct StartupOptions {
  QString source;
  QString destination;
  QString backup;
  QString mode = QStringLiteral("update");
  bool compare = false;
  bool noLogFile = false;
  QString logFile;
  bool version = false;
  bool help = false;
  QStringList qtArguments;

  static StartupOptions parse(const QStringList& arguments, QString* error);
  static void setupParser(QCommandLineParser& parser);
  static void printHelp();
  static void printVersion();
};

}  // namespace qt_rsync::app
