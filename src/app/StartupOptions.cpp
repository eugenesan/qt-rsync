#include "app/StartupOptions.h"

namespace qt_rsync::app {

void StartupOptions::setupParser(QCommandLineParser& p) {
  p.setApplicationDescription(QStringLiteral("A pure Qt frontend for rsync"));
  p.addHelpOption();
  p.addVersionOption();
  p.addOptions({
      {{QStringLiteral("dir-pair"), QStringLiteral("d")},
       QStringLiteral("populate Source and Destination"),
       QStringLiteral("SOURCE DESTINATION")},
      {QStringLiteral("backup"), QStringLiteral("populate the backup directory"),
       QStringLiteral("DIRECTORY")},
      {QStringLiteral("mode"),
       QStringLiteral("select update, mirror or new-only"),
       QStringLiteral("MODE"), QStringLiteral("update")},
      {QStringLiteral("compare"),
       QStringLiteral("start a comparison after the event loop initializes")},
      {QStringLiteral("log-file"), QStringLiteral("override the session log path"),
       QStringLiteral("PATH")},
      {QStringLiteral("no-log-file"),
       QStringLiteral("disable persistent session logging for this launch")},
  });
}

StartupOptions StartupOptions::parse(const QStringList& arguments, QString* error) {
  StartupOptions result;
  QCommandLineParser p;
  setupParser(p);
  const bool ok = p.parse(arguments);
  if (!ok && error) *error = p.errorText();
  result.qtArguments = p.positionalArguments();
  if (p.isSet(QStringLiteral("dir-pair"))) {
    // QCommandLineParser consumes one value per option flag; the second
    // operand arrives as a positional argument.
    const auto pair = p.values(QStringLiteral("dir-pair"));
    if (!pair.isEmpty()) result.source = pair.first();
    if (pair.size() > 1) {
      result.destination = pair.at(1);
    } else if (!result.qtArguments.isEmpty()) {
      result.destination = result.qtArguments.first();
    }
  }
  result.backup = p.value(QStringLiteral("backup"));
  result.mode = p.value(QStringLiteral("mode"));
  result.compare = p.isSet(QStringLiteral("compare"));
  result.logFile = p.value(QStringLiteral("log-file"));
  result.noLogFile = p.isSet(QStringLiteral("no-log-file"));
  result.version = p.isSet(QStringLiteral("version"));
  result.help = p.isSet(QStringLiteral("help"));
  return result;
}

void StartupOptions::printHelp() {
  QCommandLineParser p;
  setupParser(p);
  p.showHelp();
}

void StartupOptions::printVersion() {
  QCommandLineParser p;
  setupParser(p);
  p.showVersion();
}

}  // namespace qt_rsync::app
