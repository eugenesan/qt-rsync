#pragma once

#include <QString>
#include <QStringList>

#include "core/Plan.h"
#include "core/Profile.h"

namespace qt_rsync::core {

struct ProcessSpec {
  QString program = "rsync";
  QStringList arguments;
  QHash<QString, QString> environment;
};

// Compare and verify always use --dry-run. Only buildSyncSpec is write-capable
// and it is still gated by the confirmation dialog in the UI layer.
ProcessSpec buildCompareSpec(const SyncProfile& profile,
                             const QString& filterFile = {});
ProcessSpec buildVerifySpec(const SyncProfile& profile,
                            const QString& filterFile = {});
ProcessSpec buildSyncSpec(const SyncProfile& profile,
                          const QString& filterFile = {});
QString explainRsyncExit(int code);

}  // namespace qt_rsync::core
