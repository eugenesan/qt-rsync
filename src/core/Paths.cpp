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
  // GenericCacheLocation avoids appending the organisation/application name
  // twice: logDirectory() already adds /qt-rsync.
  return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
}

QString profilesDirectory() { return configHome() + "/qt-rsync/profiles"; }

QString settingsPath() { return stateHome() + "/qt-rsyncstaterc"; }

QString statePath() { return stateHome() + "/qt-rsyncstaterc"; }

QString configSettingsPath() { return configHome() + "/qt-rsync/settingsrc"; }

QString logDirectory() { return cacheHome() + "/qt-rsync"; }

QString appLogPath(const QString& profileName) {
  return logDirectory() + "/app-" + profileName + ".log";
}

QString rsyncLogPath(const QString& profileName) {
  return logDirectory() + "/rsync-" + profileName + ".log";
}

}  // namespace qt_rsync::core::paths
