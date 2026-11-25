#include "app/StartupOptions.h"

namespace qt_rsync::app {

void StartupOptions::setupParser(QCommandLineParser& p) {
  p.setApplicationDescription(QStringLiteral("A pure Qt frontend for rsync"));
  p.addHelpOption();
  p.addVersionOption();
  p.addOptions({
      {{QStringLiteral("dir-pair"), QStringLiteral("d")},
       QStringLiteral("populate Source and Destination"),
       QStringLiteral("SOURCE")},
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
  // Qt's own platform/style switches are forwarded to QApplication. They are
  // not application options, so separate them before QCommandLineParser sees
  // them (it rejects unknown switches).
  const QStringList known{QStringLiteral("--dir-pair"), QStringLiteral("-d"),
                          QStringLiteral("--backup"), QStringLiteral("--mode"),
                          QStringLiteral("--compare"), QStringLiteral("--log-file"),
                          QStringLiteral("--no-log-file"), QStringLiteral("--version"),
                          QStringLiteral("-v"), QStringLiteral("--help"),
                          QStringLiteral("-h")};
  QStringList appArguments;
  QStringList qtArgs;
  // QCommandLineParser::parse() expects the program name as argument zero,
  // exactly as QCoreApplication::arguments() provides it.
  for (int index = 0; index < arguments.size(); ++index) {
    const QString arg = arguments.at(index);
    if (arg.startsWith('-') && !known.contains(arg)) {
      qtArgs.append(arg);
      if (index + 1 < arguments.size() &&
          !arguments.at(index + 1).startsWith('-')) {
        qtArgs.append(arguments.at(++index));
      }
      continue;
    }
    appArguments.append(arg);
  }
  const bool ok = p.parse(appArguments);
  if (!ok && error) *error = p.errorText();
  QStringList positional = p.positionalArguments();
  if (p.isSet(QStringLiteral("dir-pair"))) {
    // QCommandLineParser consumes one value per option flag; the second
    // operand arrives as a positional argument.
    const auto pair = p.values(QStringLiteral("dir-pair"));
    if (!pair.isEmpty()) result.source = pair.first();
    if (pair.size() > 1) {
      result.destination = pair.at(1);
    } else if (!positional.isEmpty()) {
      result.destination = positional.takeFirst();
    }
  }
  result.qtArguments = positional + qtArgs;
  result.backup = p.value(QStringLiteral("backup"));
  result.mode = p.value(QStringLiteral("mode"));
  result.compare = p.isSet(QStringLiteral("compare"));
  result.logFile = p.value(QStringLiteral("log-file"));
  result.noLogFile = p.isSet(QStringLiteral("no-log-file"));
  result.version = p.isSet(QStringLiteral("version"));
  result.help = p.isSet(QStringLiteral("help"));
  if (result.mode != QStringLiteral("update") &&
      result.mode != QStringLiteral("mirror") &&
      result.mode != QStringLiteral("new-only")) {
    if (error != nullptr) {
      *error = QStringLiteral("Invalid mode: %1").arg(result.mode);
    }
    result.mode = QStringLiteral("update");
    return result;
  }
  if (result.compare &&
      (result.source.isEmpty() || result.destination.isEmpty())) {
    if (error != nullptr) {
      *error = QStringLiteral("--compare requires --dir-pair SOURCE DESTINATION");
    }
    result.compare = false;
  }
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
