#include "core/Profile.h"

#include <QCryptographicHash>

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

QString SyncProfile::fingerprint() const {
  // Identity fields (id, name, description, schema_version) are excluded:
  // ad-hoc profiles get a new UUID on every reconstruction.
  QStringList parts;
  parts << source.rsyncOperand() << destination.rsyncOperand();
  parts << QString::number(static_cast<int>(transfer.mode));
  parts << QString::number(static_cast<int>(transfer.deleteTiming));
  parts << QString::number(transfer.deletionEnabled ? 1 : 0);
  parts << QString::number(transfer.ignoreExisting ? 1 : 0);
  parts << backup.directory << QString::number(backup.protectNested ? 1 : 0);
  parts << QString::number(static_cast<int>(conflict));
  parts << filterText;
  const QByteArray payload = parts.join(QChar('\x1f')).toUtf8();
  return QString::fromLatin1(
      QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QVector<FilterRule> rulesFromText(const QString& text) {
  return filterTextToRules(text);
}

}  // namespace qt_rsync::core
