#pragma once

#include <QString>

namespace qt_rsync::core::paths {

// XDG layout retained from qpy-rsync so settings, profiles and logs can be
// migrated or inspected with the same tools.
QString configHome();
QString stateHome();
QString cacheHome();
QString profilesDirectory();
QString settingsPath();
QString statePath();
QString configSettingsPath();
QString logDirectory();
QString appLogPath(const QString& profileName);
QString rsyncLogPath(const QString& profileName);

}  // namespace qt_rsync::core::paths
