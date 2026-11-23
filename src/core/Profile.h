#pragma once

#include <QString>

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

struct TransferOptions {
  SyncMode mode = SyncMode::Update;
  DeleteTiming deleteTiming = DeleteTiming::During;
  bool deletionEnabled = false;
  bool ignoreExisting = false;
};

struct BackupOptions {
  QString directory;
  bool protectNested = true;
};

struct SyncProfile {
  QString id;
  QString name;
  Endpoint source;
  Endpoint destination;
  TransferOptions transfer;
  BackupOptions backup;
  ConflictPolicy conflict = ConflictPolicy::Override;
  QString filterText;
  QVector<FilterRule> filters;
  QString schemaVersion = "1";

  [[nodiscard]] bool mirrorEnabled() const;
  [[nodiscard]] QString fingerprint() const;
};

QVector<FilterRule> rulesFromText(const QString& text);

}  // namespace qt_rsync::core
