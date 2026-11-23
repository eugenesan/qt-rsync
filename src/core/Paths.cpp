#include "core/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace qt_rsync::core::paths {

QString configHome() {
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QString stateHome() {
  const QByteArray value = qgetenv("XDG_STATE_HOME");
  return value.isEmpty() ? QDir::homePath() + "/.local/state"
                         : QString::fromLocal8Bit(value);
}

QString cacheHome() {
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString profilesDirectory() { return configHome() + "/qpy-rsync/profiles"; }

QString settingsPath() { return stateHome() + "/qpy-rsyncstaterc"; }

QString statePath() { return stateHome() + "/qpy-rsyncstaterc"; }

QString configSettingsPath() { return configHome() + "/qpy-rsync/settingsrc"; }

QString logDirectory() { return cacheHome() + "/qpy-rsync"; }

QString appLogPath(const QString& profileName) {
  return logDirectory() + "/app-" + profileName + ".log";
}

QString rsyncLogPath(const QString& profileName) {
  return logDirectory() + "/rsync-" + profileName + ".log";
}

}  // namespace qt_rsync::core::paths
