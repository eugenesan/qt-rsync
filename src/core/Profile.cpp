#include "core/Profile.h"

namespace qt_rsync::core {

QString Endpoint::rsyncOperand() const {
  if (kind == EndpointKind::Ssh && !host.isEmpty()) {
    return user.isEmpty() ? host + ":" + path : user + "@" + host + ":" + path;
  }
  return path;
}

bool SyncProfile::mirrorEnabled() const {
  return transfer.mode == SyncMode::Mirror || transfer.deletionEnabled;
}

QString SyncProfile::fingerprint() const { return {}; }

QVector<FilterRule> rulesFromText(const QString& text) {
  return filterTextToRules(text);
}

}  // namespace qt_rsync::core
