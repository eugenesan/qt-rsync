#include "core/Command.h"

namespace qt_rsync::core {

ProcessSpec buildCompareSpec(const SyncProfile&, const QString&) { return {}; }
ProcessSpec buildVerifySpec(const SyncProfile&, const QString&) { return {}; }
ProcessSpec buildSyncSpec(const SyncProfile&, const QString&) { return {}; }

QString explainRsyncExit(int code) {
  return QStringLiteral("rsync exited with status %1.").arg(code);
}

}  // namespace qt_rsync::core
