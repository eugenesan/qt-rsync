#include "core/Parser.h"

#include <QRegularExpression>

namespace qt_rsync::core {
namespace {

const QString kSeparator = QStringLiteral("|qpy-rsync|");

Kind kindFromCode(const QString& code) {
  if (code.startsWith(QLatin1String("*deleting"))) return Kind::Unknown;
  if (code.size() < 2) return Kind::Unknown;
  switch (code.at(1).toLatin1()) {
    case 'f': return Kind::File;
    case 'd': return Kind::Directory;
    case 'L': return Kind::Symlink;
    case 'D': return Kind::Device;
    case 'S': return Kind::Special;
    default: return Kind::Unknown;
  }
}

bool isItemizedCode(const QString& code) {
  return code.startsWith(QLatin1String("*deleting")) || code.size() == 11;
}

}  // namespace

QString describeItemized(const QString& code) {
  if (code.startsWith(QLatin1String("*deleting"))) {
    return QStringLiteral("destination-only object will be deleted");
  }
  if (code.size() < 11) return QStringLiteral("unrecognized rsync action");
  const QString attributes = code.mid(2, 9);
  if (attributes.contains('+')) return QStringLiteral("new object");
  static const QVector<QPair<int, QString>> labels{
      {0, QStringLiteral("checksum/value")},
      {1, QStringLiteral("size")},
      {2, QStringLiteral("modification time")},
      {3, QStringLiteral("permissions")},
      {4, QStringLiteral("owner")},
      {5, QStringLiteral("group")},
      {6, QStringLiteral("ACL")},
      {7, QStringLiteral("extended attributes")},
  };
  QStringList reasons;
  for (const auto& label : labels) {
    if (label.first < attributes.size() &&
        attributes.at(label.first) != QLatin1Char('.') &&
        attributes.at(label.first) != QLatin1Char(' ')) {
      reasons << label.second;
    }
  }
  return reasons.isEmpty() ? QStringLiteral("rsync reported a change")
                           : reasons.join(QStringLiteral(", "));
}

std::optional<Change> parseRecord(const QString& rawLine, bool deleteEnabled) {
  const QString line = rawLine.trimmed();
  if (line.isEmpty() || !line.contains(kSeparator)) return std::nullopt;
  const QStringList parts = line.split(kSeparator, Qt::KeepEmptyParts);
  if (parts.size() != 4) return std::nullopt;
  const QString code = parts.at(0);
  if (!isItemizedCode(code)) return std::nullopt;
  const QString sizeText = parts.at(1).trimmed();
  const QString modifiedTime = parts.at(2);
  const QString nameAndLink = parts.at(3);
  if (nameAndLink.contains(QChar('\0'))) return std::nullopt;

  const bool deleting = code.startsWith(QLatin1String("*deleting"));
  Change change;
  change.itemizedCode = code;
  change.reason = describeItemized(code);
  if (deleting) {
    change.action = deleteEnabled ? Action::Delete : Action::DestinationOnly;
    change.kind = nameAndLink.endsWith('/') ? Kind::Directory : Kind::File;
  } else {
    bool ok = false;
    const qlonglong size = sizeText.toLongLong(&ok);
    change.size = ok ? size : 0;
    change.modifiedTime = modifiedTime.isEmpty() ? QString() : modifiedTime;
    change.kind = kindFromCode(code);
    const QString attributes = code.mid(2);
    const char marker = code.isEmpty() ? ' ' : code.at(0).toLatin1();
    if (attributes.contains('+')) {
      change.action = Action::Create;
    } else if (marker == '>' || marker == '<' || marker == 'c' || marker == 'h') {
      change.action = Action::UpdateContent;
    } else {
      bool metadataChanged = false;
      for (const QChar character : attributes) {
        if (character != QLatin1Char('.') && character != QLatin1Char(' ')) {
          metadataChanged = true;
          break;
        }
      }
      change.action = metadataChanged ? Action::UpdateMetadata : Action::NoChange;
    }
  }

  QString path = nameAndLink;
  if (change.kind == Kind::Symlink) {
    const int marker = path.indexOf(QStringLiteral(" -> "));
    if (marker >= 0) {
      change.linkTarget = path.mid(marker + 4);
      path = path.left(marker);
    }
  }
  if (path == QLatin1String(".") || path == QLatin1String("./")) {
    return std::nullopt;
  }
  if (path.isEmpty()) return std::nullopt;
  change.path = path;
  return change;
}

QVector<Change> parseInventory(const QString& text) {
  QVector<Change> changes;
  static const QRegularExpression line(
      QStringLiteral("^([-dDL])\\S*\\s+([\\d,]+)\\s+(\\d{4}/\\d{2}/\\d{2})\\s+"
                     "(\\d{2}:\\d{2}:\\d{2})\\s+(.+)$"));
  for (const QString& entry : text.split('\n')) {
    const auto match = line.match(entry.trimmed());
    if (!match.hasMatch()) continue;
    QString path = match.captured(5).trimmed();
    if (path.isEmpty() || path == QLatin1String(".") || path == QLatin1String("./")) continue;
    while (path.endsWith('/')) path.chop(1);
    Change change;
    change.path = path;
    change.action = Action::NoChange;
    change.kind = match.captured(1) == QLatin1String("d") ? Kind::Directory : Kind::File;
    change.itemizedCode = QStringLiteral("=");
    bool ok = false;
    const qlonglong size = match.captured(2).remove(',').toLongLong(&ok);
    if (ok) change.size = size;
    change.modifiedTime =
        match.captured(3) + QLatin1Char('-') + match.captured(4);
    changes.append(change);
  }
  return changes;
}

ParseResult parseOutput(const QString& text, bool deleteEnabled) {
  ParseResult result;
  const QStringList lines = text.split('\n');
  for (const QString& line : lines) {
    auto change = parseRecord(line, deleteEnabled);
    if (change.has_value()) {
      result.changes.append(*change);
    } else if (!line.trimmed().isEmpty() && !line.contains(kSeparator)) {
      result.diagnostics.append(line.trimmed());
    }
  }
  return result;
}

}  // namespace qt_rsync::core
