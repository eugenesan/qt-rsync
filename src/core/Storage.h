#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>

#include "core/Profile.h"

namespace qt_rsync::core::storage {

class FileLock {
 public:
  explicit FileLock(QString path, int timeoutMs = 0);

  bool acquire();
  void release();
  [[nodiscard]] QString lockPath() const;
  [[nodiscard]] bool owned() const { return owned_; }

 private:
  QString path_;
  int timeoutMs_;
  bool owned_ = false;
};

qint64 readLockPid(const QString& path);
bool pidAlive(qint64 pid);

// Stable preferences live in config; transient state in state. The split is
// inherited from qpy-rsync and keyed on the same prefixes.
QStringList actionColourKeys();
void removeObsoleteEncodedSettings();
QString encodeIntList(const QList<int>& values);
QList<int> decodeIntList(const QString& text);

class Settings {
 public:
  Settings();

  [[nodiscard]] QVariant value(const QString& key,
                               const QVariant& fallback = {}) const;
  void setValue(const QString& key, const QVariant& value);
  void remove(const QString& key);
  void sync();
  void clear();
  [[nodiscard]] QStringList allKeys() const;

  [[nodiscard]] QString statePath() const;
  [[nodiscard]] QString configPath() const;

 private:
  static bool isConfigKey(const QString& key);
  QSettings state_;
  QSettings config_;
};

QString profileToJson(const SyncProfile& profile);
SyncProfile profileFromJson(const QString& payload);
void saveProfile(const QString& path, const SyncProfile& profile);
SyncProfile loadProfile(const QString& path);
QStringList listProfiles();
QString currentProfilePath();
void setCurrentProfilePath(const QString& path);

}  // namespace qt_rsync::core::storage
