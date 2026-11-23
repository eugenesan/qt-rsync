#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include "core/Profile.h"

namespace qt_rsync::core {

struct ProcessSpec {
  QString program;
  QStringList arguments;
  QHash<QString, QString> environment;

  [[nodiscard]] QStringList argv() const;
};

// Compare and verify always use --dry-run. Only buildSyncSpec is write-capable
// and it is still gated by the confirmation dialog in the UI layer.
ProcessSpec buildCompareSpec(const SyncProfile& profile,
                             const QString& filterFile = {});
ProcessSpec buildVerifySpec(const SyncProfile& profile,
                            const QString& filterFile = {});
ProcessSpec buildSyncSpec(const SyncProfile& profile,
                          const QString& filterFile = {});
ProcessSpec buildInventorySpec(const SyncProfile& profile, const Endpoint& endpoint);
QStringList buildSyncArgv(const SyncProfile& profile,
                          const QString& filterFile = {});
QString explainRsyncExit(int code);

}  // namespace qt_rsync::core
