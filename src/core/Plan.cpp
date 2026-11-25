#include "core/Plan.h"

#include <QCryptographicHash>

namespace qt_rsync::core {
namespace {

QString hashHex(const QByteArray& payload) {
  return QString::fromLatin1(
      QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

std::uint32_t crc32(const QByteArray& bytes) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (unsigned char byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
  }
  return ~crc;
}

QString normalized(const QString& path) {
  QString result = path;
  while (result.endsWith('/')) result.chop(1);
  return result;
}

// Walk every metadata entry once and accumulate its size into each ancestor.
// The old code scanned the entire metadata table for every deleted directory,
// which was O(deletes * metadata) on a half-million-row comparison.
QHash<QString, std::int64_t> buildDescendantSizes(
    const DestinationMetadata& metadata) {
  QHash<QString, std::int64_t> sums;
  for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
    const std::int64_t size = it.value().first.value_or(0);
    QString path = normalized(it.key());
    int slash = path.lastIndexOf('/');
    while (slash >= 0) {
      const QString ancestor = path.left(slash);
      sums[ancestor] += size;
      if (slash == 0) break;
      slash = path.lastIndexOf('/', slash - 1);
    }
  }
  return sums;
}

std::int64_t destinationSubtreeSize(
    const QString& path, Kind kind, const DestinationMetadata& metadata,
    const QHash<QString, std::int64_t>& descendantSizes) {
  const QString clean = normalized(path);
  const auto own = metadata.value(clean).first;
  if (kind != Kind::Directory) return own.value_or(0);
  const std::int64_t sum = descendantSizes.value(clean, 0);
  if (sum > 0) return sum;
  return own.value_or(0);
}

QString patternFor(const PlanItem& item) {
  const QString path = item.path.trimmed();
  return QStringLiteral("/") + path +
         (item.kind == Kind::Directory ? QStringLiteral("/***") : QString());
}

}  // namespace

QString actionName(Action action) {
  switch (action) {
    case Action::Create: return QStringLiteral("create");
    case Action::UpdateContent: return QStringLiteral("update_content");
    case Action::UpdateMetadata: return QStringLiteral("update_metadata");
    case Action::Delete: return QStringLiteral("delete");
    case Action::DestinationOnly: return QStringLiteral("destination_only");
    case Action::NoChange: return QStringLiteral("no_change");
    case Action::Message: return QStringLiteral("message");
  }
  return {};
}

QString kindName(Kind kind) {
  switch (kind) {
    case Kind::File: return QStringLiteral("file");
    case Kind::Directory: return QStringLiteral("directory");
    case Kind::Symlink: return QStringLiteral("symlink");
    case Kind::Device: return QStringLiteral("device");
    case Kind::Special: return QStringLiteral("special");
    case Kind::Unknown: return QStringLiteral("unknown");
  }
  return {};
}

QString PlanItem::identity() const {
  return path + QChar('\x1f') + actionName(comparisonAction) +
         QChar('\x1f') + kindName(kind);
}

QVector<PlanItem> ExecutionPlan::includedItems() const {
  QVector<PlanItem> result;
  for (const PlanItem& item : items) {
    if (item.included) result.append(item);
  }
  return result;
}

QString planItemsFingerprint(const QVector<PlanItem>& items) {
  QByteArray payload;
  payload.reserve(static_cast<qsizetype>(items.size()) * 24);
  for (const PlanItem& item : items) {
    payload += item.path.toUtf8();
    payload += actionName(item.action).toUtf8();
    payload += kindName(item.kind).toUtf8();
    payload += item.included ? "1" : "0";
    if (item.syntheticDelete) payload += "D";
  }
  return hashHex(payload);
}

QString decisionSignature(const Decisions& decisions) {
  QString payload;
  const auto items = decisions.explicitItems();
  for (const auto& item : items) {
    payload += item.first;
    payload += QChar('=');
    payload += QString::number(static_cast<int>(item.second));
    payload += QChar('\n');
  }
  return hashHex(payload.toUtf8());
}

QString comparisonSignature(const QVector<Change>& changes,
                            const QSet<QString>& extraPaths) {
  std::uint32_t accumulator = 0;
  int count = 0;
  for (const Change& change : changes) {
    const QString path = normalized(change.path);
    if (!Decisions::selectable(change) && !extraPaths.contains(path)) continue;
    QByteArray key = path.toUtf8();
    key += '\0';
    key += actionName(change.action).toUtf8();
    key += '\0';
    key += kindName(change.kind).toUtf8();
    key += '\0';
    accumulator ^= crc32(key);
    ++count;
  }
  return QString::number(count) + ':' +
         QString::number(accumulator, 16).rightJustified(8, '0');
}

ExecutionPlan buildExecutionPlan(const SyncProfile& profile,
                                 const QVector<Change>& changes,
                                 const Decisions& decisions,
                                 const DestinationMetadata& metadata) {
  ExecutionPlan plan;
  const bool hasExplicit = decisions.hasExplicit();
  QHash<QString, Kind> deleteKinds;
  QSet<QString> deletePaths;

  for (const Change& change : changes) {
    const Action action = change.action;
    std::optional<Decision> decision;
    if (hasExplicit) decision = decisions.effective(change);
    const bool deleteDecision = decision == Decision::Delete;
    if ((action == Action::NoChange || action == Action::Message) &&
        !deleteDecision) {
      continue;
    }
    const QString clean = normalized(change.path);
    PlanItem item;
    item.comparisonAction = action;
    item.kind = change.kind;
    item.size = change.size;
    if (deleteDecision) {
      item.included = true;
      item.action = Action::Delete;
    } else {
      item.included = hasExplicit ? *decision != Decision::Exclude
                                  : action != Action::DestinationOnly;
      item.action = (action == Action::DestinationOnly && item.included)
                        ? Action::Delete
                        : action;
    }
    item.destructive = item.included && item.action == Action::Delete;
    item.syntheticDelete = deleteDecision && action != Action::Delete;
    item.path = clean;
    plan.items.append(item);

    if (item.included) {
      ++plan.summary.included;
      if (item.destructive) ++plan.summary.destructive;
      const std::int64_t size = item.size.value_or(0);
      plan.summary.includedSize += size;
      if (item.action == Action::Create || item.action == Action::UpdateContent) {
        plan.summary.totalSize += size;
      }
      switch (item.action) {
        case Action::Create: ++plan.summary.creates; break;
        case Action::UpdateContent: ++plan.summary.updates; break;
        case Action::UpdateMetadata: ++plan.summary.metadata; break;
        case Action::Delete:
          ++plan.summary.deletions;
          deletePaths.insert(clean);
          deleteKinds.insert(clean, change.kind);
          break;
        default: break;
      }
    } else if (action == Action::DestinationOnly) {
      ++plan.summary.retainedDestinationOnly;
    } else {
      ++plan.summary.excluded;
    }
  }

  const QHash<QString, std::int64_t> descendantSizes =
      buildDescendantSizes(metadata);
  for (const QString& path : deletePaths) {
    bool hasDeleteAncestor = false;
    QString parent = path.section('/', 0, -2);
    while (!parent.isEmpty()) {
      if (deletePaths.contains(parent)) {
        hasDeleteAncestor = true;
        break;
      }
      parent = parent.section('/', 0, -2);
    }
    if (!hasDeleteAncestor) {
      plan.summary.deletedSize += destinationSubtreeSize(
          path, deleteKinds.value(path, Kind::File), metadata, descendantSizes);
    }
  }

  plan.summary.modified = decisions.modifiedCount(changes);
  plan.summary.backupEnabled = !profile.backup.directory.isEmpty();
  plan.itemsFingerprint = planItemsFingerprint(plan.items);
  plan.decisionSignature = decisionSignature(decisions);
  plan.profileFingerprint = profile.fingerprint();
  plan.sourceOperand = profile.source.rsyncOperand();
  plan.destinationOperand = profile.destination.rsyncOperand();
  QSet<QString> syntheticPaths;
  for (const PlanItem& item : plan.items) {
    if (item.syntheticDelete) syntheticPaths.insert(item.path);
  }
  plan.comparisonSignature = comparisonSignature(changes, syntheticPaths);
  return plan;
}

QStringList decisionFilterLines(const ExecutionPlan& plan) {
  QStringList lines;
  // Sender-side deletes lead: rsync evaluates filter rules first-match-wins.
  for (const PlanItem& item : plan.items) {
    if (!item.syntheticDelete) continue;
    const QString path = item.path.trimmed();
    lines << QStringLiteral("-s /") + path +
                 (item.kind == Kind::Directory ? QStringLiteral("/")
                                               : QString());
  }

  QSet<QString> excludedDirectories;
  for (const PlanItem& item : plan.items) {
    if (!item.included && item.kind == Kind::Directory) {
      excludedDirectories.insert(item.path.trimmed());
    }
  }
  QVector<PlanItem> includedOverrides;
  for (const PlanItem& item : plan.items) {
    if (!item.included) continue;
    const QStringList parts = item.path.trimmed().split('/');
    for (int position = 1; position < parts.size(); ++position) {
      if (excludedDirectories.contains(
              QStringList(parts.mid(0, position)).join('/'))) {
        includedOverrides.append(item);
        break;
      }
    }
  }
  QSet<QString> traversal;
  for (const PlanItem& item : includedOverrides) {
    const QStringList parts = item.path.trimmed().split('/');
    for (int position = 1; position < parts.size(); ++position) {
      traversal.insert('/' + QStringList(parts.mid(0, position)).join('/') + '/');
    }
  }
  QStringList sortedTraversal = traversal.values();
  sortedTraversal.sort();
  lines << sortedTraversal;
  for (const PlanItem& item : includedOverrides) {
    lines << QStringLiteral("+ ") + patternFor(item);
  }
  for (const PlanItem& item : plan.items) {
    if (item.included) continue;
    if (item.action == Action::Delete || item.action == Action::DestinationOnly) {
      lines << QStringLiteral("P ") + patternFor(item);
    }
    lines << QStringLiteral("- ") + patternFor(item);
  }
  return lines;
}

QString decisionFilterText(const ExecutionPlan& plan) {
  return decisionFilterLines(plan).join('\n');
}

PlanMismatch comparePlanToChanges(const ExecutionPlan& plan,
                                  const QVector<Change>& changes,
                                  const Decisions* decisions) {
  QSet<QString> expected;
  for (const PlanItem& item : plan.items) {
    if (!item.included) continue;
    expected.insert(item.path + QChar('\x1f') + actionName(item.action) +
                    QChar('\x1f') + kindName(item.kind));
  }
  QSet<QString> actual;
  for (const Change& change : changes) {
    if (change.action == Action::NoChange || change.action == Action::Message ||
        change.action == Action::DestinationOnly) {
      continue;
    }
    if (decisions != nullptr &&
        decisions->inherited(change.path) == Decision::Exclude) {
      continue;
    }
    actual.insert(normalized(change.path) + QChar('\x1f') +
                  actionName(change.action) + QChar('\x1f') +
                  kindName(change.kind));
  }
  PlanMismatch result;
  // QSet::subtract mutates its receiver, so compare two independent copies.
  QSet<QString> missing = expected;
  missing.subtract(actual);
  QSet<QString> unexpected = actual;
  unexpected.subtract(expected);
  for (const QString& key : missing) result.missing << key;
  for (const QString& key : unexpected) result.unexpected << key;
  result.missing.sort();
  result.unexpected.sort();
  return result;
}

PlanFreshness checkPlanFreshness(const ExecutionPlan& plan,
                                 const SyncProfile& profile,
                                 const QVector<Change>& changes,
                                 const QString& signature) {
  PlanFreshness result;
  if (plan.profileFingerprint != profile.fingerprint()) {
    result.reasons << QStringLiteral("profile options changed");
  }
  if (plan.sourceOperand != profile.source.rsyncOperand()) {
    result.reasons << QStringLiteral("source endpoint changed");
  }
  if (plan.destinationOperand != profile.destination.rsyncOperand()) {
    result.reasons << QStringLiteral("destination endpoint changed");
  }
  if (!signature.isEmpty() && plan.decisionSignature != signature) {
    result.reasons << QStringLiteral("decisions changed");
  }

  QSet<QString> syntheticPaths;
  for (const PlanItem& item : plan.items) {
    if (item.syntheticDelete) syntheticPaths.insert(item.path);
  }
  const QString currentComparison = comparisonSignature(changes, syntheticPaths);
  if (plan.comparisonSignature.isEmpty() ||
      plan.comparisonSignature != currentComparison) {
    QSet<QString> existingPaths;
    for (const Change& change : changes) existingPaths.insert(normalized(change.path));
    QSet<QString> current;
    for (const Change& change : changes) {
      const QString path = normalized(change.path);
      if (Decisions::selectable(change) || syntheticPaths.contains(path)) {
        current.insert(path + QChar('\x1f') + actionName(change.action) +
                       QChar('\x1f') + kindName(change.kind));
      }
    }
    QSet<QString> planned;
    for (const PlanItem& item : plan.items) {
      if (!item.syntheticDelete || existingPaths.contains(item.path)) {
        planned.insert(item.identity());
      }
    }
    if (current != planned) result.reasons << QStringLiteral("comparison decisions changed");
  }
  result.fresh = result.reasons.isEmpty();
  return result;
}

}  // namespace qt_rsync::core
