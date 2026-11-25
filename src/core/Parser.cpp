#include "core/Parser.h"

#include <QRegularExpression>

#include <cmath>

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

const QStringList kSummaryPrefixes{
    QStringLiteral("Number of files"),
    QStringLiteral("Number of created files"),
    QStringLiteral("Number of deleted files"),
    QStringLiteral("Number of regular files transferred"),
    QStringLiteral("Total file size:"),
    QStringLiteral("Total transferred file size:"),
    QStringLiteral("Literal data:"),
    QStringLiteral("Matched data:"),
    QStringLiteral("File list size:"),
    QStringLiteral("File list generation time:"),
    QStringLiteral("File list transfer time:"),
    QStringLiteral("Total bytes sent:"),
    QStringLiteral("Total bytes received:"),
    QStringLiteral("sent "),
    QStringLiteral("received "),
    QStringLiteral("Speedup is"),
    QStringLiteral("sending incremental file list"),
};

}  // namespace

std::optional<SyncProgress> parseSyncProgress(const QString& rawLine) {
  const QString line = rawLine.trimmed();
  if (!line.contains(QLatin1String("to-chk=")) &&
      !line.contains(QLatin1String("ir-chk="))) {
    return std::nullopt;
  }
  static const QRegularExpression counter(
      QStringLiteral(R"((?:xfr#(\d+),\s*)?(?:ir-chk|to-chk)=(\d+)/(\d+))"));
  const QRegularExpressionMatch match = counter.match(line);
  if (!match.hasMatch()) return std::nullopt;
  SyncProgress result;
  result.filesCompleted = match.captured(1).toInt();
  result.filesRemaining = match.captured(2).toInt();
  result.filesTotal = match.captured(3).toInt();
  if (result.filesTotal > 0) {
    result.percent = static_cast<int>(
        std::round(100.0 * (result.filesTotal - result.filesRemaining) /
                   result.filesTotal));
  } else {
    const int denominator = result.filesCompleted + result.filesRemaining;
    result.percent = denominator > 0
                         ? static_cast<int>(std::round(
                               100.0 * result.filesCompleted / denominator))
                         : 0;
  }
  result.percent = qBound(0, result.percent, 100);
  return result;
}

bool isRsyncSummaryLine(const QString& rawLine) {
  const QString line = rawLine.trimmed();
  if (line.contains(QLatin1String("error"), Qt::CaseInsensitive) ||
      line.contains(QLatin1String("failed"), Qt::CaseInsensitive) ||
      line.contains(QLatin1String("warning"), Qt::CaseInsensitive)) {
    return true;
  }
  for (const QString& prefix : kSummaryPrefixes) {
    if (line.startsWith(prefix)) return true;
  }
  return false;
}

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

bool looksLikeItemizedRecord(const QString& rawLine) {
  const qsizetype first = rawLine.indexOf(kSeparator);
  if (first <= 0) return false;
  const qsizetype second = rawLine.indexOf(kSeparator, first + kSeparator.size());
  if (second < 0) return false;
  const qsizetype third = rawLine.indexOf(kSeparator, second + kSeparator.size());
  if (third < 0) return false;
  if (rawLine.indexOf(kSeparator, third + kSeparator.size()) >= 0) return false;
  const QString code = rawLine.left(first).trimmed();
  if (!isItemizedCode(code)) return false;
  const QString path = rawLine.mid(third + kSeparator.size()).trimmed();
  return !path.isEmpty() && path != QLatin1String(".") &&
         path != QLatin1String("./");
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
  result.changes.reserve(lines.size());
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
