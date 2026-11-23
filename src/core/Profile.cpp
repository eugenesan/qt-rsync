#include "core/Profile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>

namespace qt_rsync::core {
namespace {

QString expandUser(const QString& value) {
  if (value == QLatin1String("~")) return QDir::homePath();
  if (value.startsWith(QLatin1String("~/"))) {
    return QDir::homePath() + value.mid(1);
  }
  return value;
}

QString endpointPath(const Endpoint& endpoint) {
  const QString path = endpoint.kind == EndpointKind::Local
                           ? expandUser(endpoint.path)
                           : endpoint.path;
  return normalizePosixPath(path);
}

bool isWithin(const QString& path, const QString& root) {
  if (!path.startsWith('/') || !root.startsWith('/')) return false;
  if (path == root) return false;
  QString trimmed = root;
  while (trimmed.endsWith('/') && trimmed.size() > 1) trimmed.chop(1);
  return path.startsWith(trimmed + '/');
}

QString relativePattern(const QString& path, const QString& root) {
  if (!isWithin(path, root)) return QString();
  QString trimmed = root;
  while (trimmed.endsWith('/') && trimmed.size() > 1) trimmed.chop(1);
  const QString relative = path.mid(trimmed.size() + 1);
  return '/' + relative + QStringLiteral("/***");
}

}  // namespace

QString Endpoint::rsyncOperand() const {
  if (kind == EndpointKind::Local) return expandUser(path);
  if (host.isEmpty()) return QString();
  const QString authority = user.isEmpty() ? host : user + '@' + host;
  return authority + ':' + path;
}

bool TransferOptions::mirrorEnabled() const {
  return mode == SyncMode::Mirror || legacyDelete;
}

QString normalizePosixPath(const QString& value) {
  QString text = value.trimmed();
  if (text.isEmpty()) return {};
  QStringList parts;
  const bool absolute = text.startsWith('/');
  for (const QString& part : text.split('/')) {
    if (part.isEmpty() || part == QLatin1String(".")) continue;
    if (part == QLatin1String("..")) {
      if (!parts.isEmpty() && parts.last() != QLatin1String("..")) {
        parts.removeLast();
      } else if (!absolute) {
        parts.append(part);
      }
      continue;
    }
    parts.append(part);
  }
  QString result = absolute ? QStringLiteral("/") : QString();
  result += parts.join('/');
  if (result.isEmpty()) return QStringLiteral(".");
  return result;
}

QString destinationBackupDirectory(const SyncProfile& profile) {
  QString directory = profile.backup.directory.trimmed();
  if (profile.destination.kind == EndpointKind::Local) {
    directory = expandUser(directory);
  }
  return normalizePosixPath(directory);
}

QStringList backupProtectionArguments(const SyncProfile& profile) {
  if (!profile.backup.enabled() || !profile.backup.protectNested) return {};
  const QString directory = destinationBackupDirectory(profile);
  QString resolved = directory;
  if (!directory.startsWith('/')) {
    const QString destination = endpointPath(profile.destination);
    resolved = destination.startsWith('/')
                   ? normalizePosixPath(destination + '/' + directory)
                   : directory;
  }
  const QString source = endpointPath(profile.source);
  const QString destination = endpointPath(profile.destination);
  const QString pattern = relativePattern(resolved, source).isEmpty()
                              ? relativePattern(resolved, destination)
                              : relativePattern(resolved, source);
  if (pattern.isEmpty()) return {};
  return {QStringLiteral("--filter"), QStringLiteral("P ") + pattern,
          QStringLiteral("--filter"), QStringLiteral("- ") + pattern};
}

QString SyncProfile::fingerprint() const {
  // Identity fields (id, name, description, schema_version) are excluded:
  // ad-hoc profiles get a new UUID on every reconstruction.
  QStringList parts;
  parts << source.rsyncOperand() << destination.rsyncOperand();
  parts << QString::number(static_cast<int>(transfer.mode));
  parts << QString::number(static_cast<int>(transfer.deleteTiming));
  parts << QString::number(transfer.legacyDelete ? 1 : 0);
  parts << QString::number(transfer.fuzzy ? 1 : 0);
  parts << QString::number(transfer.copyLinks ? 1 : 0);
  parts << QString::number(transfer.wholeFile ? 1 : 0);
  parts << QString::number(transfer.relative ? 1 : 0);
  parts << QString::number(transfer.compress ? 1 : 0);
  parts << QString::number(transfer.checksum ? 1 : 0);
  parts << transfer.partialDir;
  parts << QString::number(transfer.inplace ? 1 : 0);
  parts << QString::number(transfer.oneFileSystem ? 1 : 0);
  parts << QString::number(transfer.sparse ? 1 : 0);
  parts << QString::number(transfer.ignoreTimes ? 1 : 0);
  parts << QString::number(preservation.archive ? 1 : 0);
  parts << QString::number(preservation.hardLinks ? 1 : 0);
  parts << QString::number(preservation.acls ? 1 : 0);
  parts << QString::number(preservation.xattrs ? 1 : 0);
  parts << QString::number(preservation.numericIds ? 1 : 0);
  parts << destinationBackupDirectory(*this);
  parts << QString::number(backup.protectNested ? 1 : 0);
  parts << QString::number(static_cast<int>(conflict));
  parts << filterText;
  const QByteArray payload = parts.join(QChar('\x1f')).toUtf8();
  return QString::fromLatin1(
      QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

}  // namespace qt_rsync::core
