#include "core/Decision.h"

#include <QSet>

#include <algorithm>

namespace qt_rsync::core {
namespace {

QString parentOf(const QString& path) {
  const int slash = path.lastIndexOf('/');
  return slash < 0 ? QString() : path.left(slash);
}

QString cleanPath(const QString& path) {
  QString result = path;
  while (result.endsWith('/')) result.chop(1);
  return result;
}

}  // namespace

QString Decisions::normalize(const QString& path) { return cleanPath(path); }

void Decisions::set(const QString& path, Decision decision) {
  const QString key = normalize(path);
  conflicts_.remove(key);
  if (decision == Decision::Inherit) {
    explicit_.remove(key);
  } else {
    explicit_.insert(key, decision);
  }
}

void Decisions::reset(const QString& path) {
  const QString key = normalize(path);
  explicit_.remove(key);
  conflicts_.remove(key);
}

void Decisions::resetSubtree(const QString& path) {
  const QString root = normalize(path);
  const QString prefix = root + '/';
  const auto keys = explicit_.keys();
  for (const QString& key : keys) {
    if (key == root || key.startsWith(prefix)) explicit_.remove(key);
  }
  const auto conflictKeys = conflicts_.keys();
  for (const QString& key : conflictKeys) {
    if (key == root || key.startsWith(prefix)) conflicts_.remove(key);
  }
}

void Decisions::clear() {
  explicit_.clear();
  conflicts_.clear();
}

void Decisions::replace(const QHash<QString, Decision>& decisions) {
  explicit_ = decisions;
}

Decision Decisions::explicitDecision(const QString& path) const {
  return explicit_.value(normalize(path), Decision::Inherit);
}

std::optional<Decision> Decisions::inherited(const QString& path) const {
  if (explicit_.isEmpty()) return std::nullopt;
  QString current = normalize(path);
  while (!current.isEmpty()) {
    const auto it = explicit_.constFind(current);
    if (it != explicit_.cend()) return it.value();
    current = parentOf(current);
  }
  const auto root = explicit_.constFind(QString());
  if (root != explicit_.cend()) return root.value();
  return std::nullopt;
}

Decision Decisions::defaultFor(const QString& path) const {
  return defaults_.value(normalize(path), Decision::Include);
}

Decision Decisions::effective(const Change& change) const {
  const auto inheritedDecision = inherited(change.path);
  if (inheritedDecision == Decision::Delete) return Decision::Delete;
  if (!selectable(change)) return Decision::Exclude;
  if (inheritedDecision.has_value()) return *inheritedDecision;
  if (change.action == Action::DestinationOnly) return Decision::Exclude;
  return Decision::Include;
}

Decision Decisions::effectivePath(const QString& path) const {
  const auto value = inherited(path);
  return value.value_or(Decision::Include);
}

bool Decisions::selectable(const Change& change) {
  return change.action != Action::NoChange && change.action != Action::Message;
}

bool Decisions::isSelectable(const Change& change) const {
  return selectable(change) || inherited(change.path) == Decision::Delete;
}

QVector<QPair<QString, Decision>> Decisions::explicitItems() const {
  QVector<QPair<QString, Decision>> items;
  items.reserve(explicit_.size());
  for (auto it = explicit_.cbegin(); it != explicit_.cend(); ++it) {
    items.append({it.key(), it.value()});
  }
  std::sort(items.begin(), items.end(),
            [](const auto& left, const auto& right) { return left.first < right.first; });
  return items;
}

QHash<QString, Decision> Decisions::explicitItemsAsHash() const {
  return explicit_;
}

QSet<QString> Decisions::excludedPaths() const {
  QSet<QString> paths;
  for (auto it = explicit_.cbegin(); it != explicit_.cend(); ++it) {
    if (it.value() == Decision::Exclude) paths.insert(it.key());
  }
  return paths;
}

Decision Decisions::defaultForPath(const QString& path,
                                   const QVector<Change>& changes) {
  const QString key = cleanPath(path);
  bool hasDescendants = false;
  bool allDestinationOnly = true;
  for (const Change& change : changes) {
    const QString candidate = cleanPath(change.path);
    if (candidate == key) {
      return change.action == Action::DestinationOnly ? Decision::Exclude
                                                       : Decision::Include;
    }
    if (candidate.startsWith(key + '/')) {
      hasDescendants = true;
      if (change.action != Action::DestinationOnly) allDestinationOnly = false;
    }
  }
  if (hasDescendants && allDestinationOnly) return Decision::Exclude;
  return Decision::Include;
}

QSet<QString> Decisions::overridePaths(const QVector<Change>& changes) const {
  QSet<QString> result;
  for (auto it = explicit_.cbegin(); it != explicit_.cend(); ++it) {
    Decision fallback = defaults_.value(it.key(), Decision::Inherit);
    if (fallback == Decision::Inherit) {
      fallback = defaultForPath(it.key(), changes);
    }
    if (it.value() != fallback) result.insert(it.key());
  }
  return result;
}

int Decisions::modifiedCount(const QVector<Change>& changes) const {
  return overridePaths(changes).size();
}

DecisionSummary Decisions::summary(const QVector<Change>& changes) const {
  DecisionSummary result;
  for (const Change& change : changes) {
    const Decision decision = effective(change);
    if (decision == Decision::Delete) {
      ++result.included;
      ++result.destructive;
      continue;
    }
    if (!selectable(change)) continue;
    if (decision == Decision::Exclude) {
      ++result.excluded;
    } else {
      ++result.included;
      if (change.action == Action::Delete) ++result.destructive;
    }
  }
  result.modified = modifiedCount(changes);
  return result;
}

bool Decisions::conflicted(const QString& path) const {
  return conflicts_.contains(normalize(path));
}

void Decisions::resolveConflict(const QString& path) {
  conflicts_.remove(normalize(path));
}

int Decisions::acceptDefaultsForConflicts() {
  const int count = conflicts_.size();
  for (const QString& path : conflicts_.keys()) explicit_.remove(path);
  conflicts_.clear();
  return count;
}

QHash<QString, Decision> Decisions::buildDefaults(
    const QHash<QString, QPair<Action, Kind>>& current) {
  QHash<QString, Decision> defaults;
  QSet<QString> nonDestination;
  for (auto it = current.cbegin(); it != current.cend(); ++it) {
    const bool destinationOnly = it.value().first == Action::DestinationOnly;
    defaults.insert(it.key(), destinationOnly ? Decision::Exclude : Decision::Include);
    if (!destinationOnly) {
      QString ancestor = it.key();
      while (!ancestor.isEmpty()) {
        nonDestination.insert(ancestor);
        ancestor = parentOf(ancestor);
      }
    }
  }
  for (auto it = current.cbegin(); it != current.cend(); ++it) {
    QString ancestor = parentOf(it.key());
    while (!ancestor.isEmpty()) {
      if (!defaults.contains(ancestor)) {
        defaults.insert(ancestor, nonDestination.contains(ancestor)
                                      ? Decision::Include
                                      : Decision::Exclude);
      }
      ancestor = parentOf(ancestor);
    }
  }
  return defaults;
}

int Decisions::reconcile(const QVector<Change>& changes, bool preserve,
                         bool detectInherited) {
  QHash<QString, QPair<Action, Kind>> current;
  for (const Change& change : changes) {
    current.insert(cleanPath(change.path), {change.action, change.kind});
  }
  const QStringList currentPaths = current.keys();
  conflicts_.clear();
  QSet<QString> stale;

  // One pass over the paths builds the set of all ancestor folders. The old
  // code rescanned every path for every explicit decision, which made a
  // bulk "delete using decisions" edit quadratic on a large comparison.
  QSet<QString> ancestorPaths;
  for (const QString& candidate : currentPaths) {
    int slash = candidate.lastIndexOf('/');
    while (slash >= 0) {
      ancestorPaths.insert(candidate.left(slash));
      if (slash == 0) break;
      slash = candidate.lastIndexOf('/', slash - 1);
    }
  }

  for (auto it = explicit_.cbegin(); it != explicit_.cend(); ++it) {
    const QString& path = it.key();
    const Decision decision = it.value();
    const bool exists = current.contains(path);
    const bool hasDescendants = ancestorPaths.contains(path);
    const bool identityChanged = identities_.contains(path) &&
                                 identities_.value(path) != current.value(path);

    if (!exists && !hasDescendants) {
      stale.insert(path);
      continue;
    }
    if (!preserve) {
      if (identityChanged || decision == Decision::Delete) stale.insert(path);
      continue;
    }
    if (decision == Decision::Delete) continue;
    if (!identityChanged) continue;
    if (decision == Decision::Exclude) continue;
    const Action newAction = exists ? current.value(path).first : Action::NoChange;
    if (newAction == Action::DestinationOnly) {
      stale.insert(path);
      conflicts_.insert(path, QStringLiteral(
          "The item is now destination-only; including it would delete it."));
    }
  }

  if (detectInherited && preserve && !identities_.isEmpty()) {
    // Index destination-only paths by every ancestor so the lookup below is
    // constant time instead of scanning all current/identity paths per folder.
    QHash<QString, QSet<QString>> currentByAncestor;
    QHash<QString, QSet<QString>> previousByAncestor;
    const auto index = [](QHash<QString, QSet<QString>>& target,
                          const QString& key) {
      int slash = key.lastIndexOf('/');
      target[key].insert(key);
      while (slash >= 0) {
        target[key.left(slash)].insert(key);
        if (slash == 0) break;
        slash = key.lastIndexOf('/', slash - 1);
      }
    };
    for (auto currentIt = current.cbegin(); currentIt != current.cend(); ++currentIt) {
      if (currentIt.value().first == Action::DestinationOnly) {
        index(currentByAncestor, currentIt.key());
      }
    }
    for (auto identityIt = identities_.cbegin(); identityIt != identities_.cend();
         ++identityIt) {
      if (identityIt.value().first == Action::DestinationOnly) {
        index(previousByAncestor, identityIt.key());
      }
    }
    for (auto it = explicit_.cbegin(); it != explicit_.cend(); ++it) {
      const QString path = it.key();
      if (stale.contains(path) || it.value() != Decision::Include) continue;
      const QSet<QString> covered = currentByAncestor.value(path);
      if (covered.isEmpty()) continue;
      QSet<QString> appearedPaths = covered;
      appearedPaths.subtract(previousByAncestor.value(path));
      const int appeared = appearedPaths.size();
      if (appeared > 0) {
        stale.insert(path);
        conflicts_.insert(path, QStringLiteral("%1 destination-only item(s) "
                                                "appeared below this included folder.")
                                     .arg(appeared));
      }
    }
  }

  for (const QString& path : stale) explicit_.remove(path);
  identities_ = current;
  defaults_ = buildDefaults(current);
  return stale.size();
}

}  // namespace qt_rsync::core
