#pragma once

#include <QString>
#include <QStringList>

#include "core/Filter.h"

namespace qt_rsync::core {

enum class EndpointKind { Local, Ssh };
enum class SyncMode { Update, Mirror, NewOnly };
enum class DeleteTiming { Before, During, Delay, After };
enum class ConflictPolicy { Override, Skip, Unselect };

struct Endpoint {
  EndpointKind kind = EndpointKind::Local;
  QString path;
  QString host;
  QString user;

  [[nodiscard]] QString rsyncOperand() const;
};

struct PreservationOptions {
  bool archive = true;
  bool hardLinks = false;
  bool acls = false;
  bool xattrs = false;
  bool numericIds = false;
};

struct TransferOptions {
  SyncMode mode = SyncMode::Update;
  bool legacyDelete = false;
  DeleteTiming deleteTiming = DeleteTiming::During;
  bool fuzzy = false;
  bool copyLinks = false;
  bool wholeFile = false;
  bool relative = false;
  bool compress = false;
  bool checksum = false;
  QString partialDir;
  bool inplace = false;
  bool oneFileSystem = false;
  bool sparse = false;
  bool ignoreTimes = false;

  [[nodiscard]] bool mirrorEnabled() const;
  [[nodiscard]] bool newOnlyEnabled() const { return mode == SyncMode::NewOnly; }
};

struct BackupOptions {
  QString directory;
  bool protectNested = true;
  [[nodiscard]] bool enabled() const { return !directory.trimmed().isEmpty(); }
};

struct SyncProfile {
  QString id;
  QString name;
  QString description;
  Endpoint source;
  Endpoint destination;
  PreservationOptions preservation;
  TransferOptions transfer;
  BackupOptions backup;
  ConflictPolicy conflict = ConflictPolicy::Override;
  QString filterText;
  QVector<FilterRule> filters;
  bool inventoryAfterCompare = false;
  int schemaVersion = 3;

  [[nodiscard]] QString fingerprint() const;
};

QString normalizePosixPath(const QString& value);
QString destinationBackupDirectory(const SyncProfile& profile);
QStringList backupProtectionArguments(const SyncProfile& profile);

}  // namespace qt_rsync::core
