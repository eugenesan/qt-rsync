#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include "core/Change.h"

namespace qt_rsync::core {

// One decision store for the whole application. The UI never keeps its own
// copy: view models query Decisions directly (the main lesson from the
// qpy-rsync hybrid phase).
enum class Decision { Inherit, Include, Exclude, Delete };

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

  [[nodiscard]] Decision explicitDecision(const QString& path) const;
  [[nodiscard]] Decision effective(const Change& change) const;
  [[nodiscard]] bool isSelectable(const Change& change) const;
  [[nodiscard]] bool hasExplicit() const { return !explicit_.isEmpty(); }
  [[nodiscard]] QVector<QPair<QString, Decision>> explicitItems() const;
  [[nodiscard]] QSet<QString> excludedPaths() const;
  [[nodiscard]] DecisionSummary summary(const QVector<Change>& changes) const;

  // Returns the number of decisions dropped because they no longer apply.
  int reconcile(const QVector<Change>& changes, bool preserve);

 private:
  QHash<QString, Decision> explicit_;
  QHash<QString, QPair<Action, Kind>> identities_;
  QHash<QString, Decision> defaults_;
  QHash<QString, QString> conflicts_;
};

}  // namespace qt_rsync::core
