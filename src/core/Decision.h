#pragma once

#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

#include "core/Change.h"

namespace qt_rsync::core {

// One decision store for the whole application. The UI never keeps its own
// copy: view models query Decisions directly (ADR-002).
enum class Decision { Inherit = 0, Include = 1, Exclude = 2, Delete = 3 };

struct DecisionSummary {
  int included = 0;
  int excluded = 0;
  int modified = 0;
  int destructive = 0;
};

class Decisions {
 public:
  void set(const QString& path, Decision decision);
  void reset(const QString& path);
  void resetSubtree(const QString& path);
  void clear();
  void replace(const QHash<QString, Decision>& decisions);

  [[nodiscard]] Decision explicitDecision(const QString& path) const;
  [[nodiscard]] std::optional<Decision> inherited(const QString& path) const;
  [[nodiscard]] Decision defaultFor(const QString& path) const;
  [[nodiscard]] Decision effective(const Change& change) const;
  [[nodiscard]] Decision effectivePath(const QString& path) const;

  static bool selectable(const Change& change);
  [[nodiscard]] bool isSelectable(const Change& change) const;
  [[nodiscard]] bool hasExplicit() const { return !explicit_.isEmpty(); }

  // Deterministic (path-sorted) snapshots used for fingerprints.
  [[nodiscard]] QVector<QPair<QString, Decision>> explicitItems() const;
  [[nodiscard]] QSet<QString> excludedPaths() const;
  [[nodiscard]] QSet<QString> overridePaths(const QVector<Change>& changes) const;
  [[nodiscard]] int modifiedCount(const QVector<Change>& changes) const;
  [[nodiscard]] DecisionSummary summary(const QVector<Change>& changes) const;

  [[nodiscard]] QHash<QString, QString> conflicts() const { return conflicts_; }
  [[nodiscard]] bool conflicted(const QString& path) const;
  void resolveConflict(const QString& path);
  [[nodiscard]] int acceptDefaultsForConflicts();

  // Returns the number of decisions dropped because they no longer apply.
  int reconcile(const QVector<Change>& changes, bool preserve,
                bool detectInherited = false);

 private:
  static QString normalize(const QString& path);
  static Decision defaultForPath(const QString& path,
                                 const QVector<Change>& changes);
  static QHash<QString, Decision> buildDefaults(
      const QHash<QString, QPair<Action, Kind>>& current);

  QHash<QString, Decision> explicit_;
  QHash<QString, QPair<Action, Kind>> identities_;
  QHash<QString, Decision> defaults_;
  QHash<QString, QString> conflicts_;
};

}  // namespace qt_rsync::core
