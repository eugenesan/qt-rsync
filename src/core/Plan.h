#pragma once

#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "core/Change.h"
#include "core/Decision.h"
#include "core/Profile.h"

namespace qt_rsync::core {

struct PlanItem {
  QString path;
  Action action = Action::NoChange;
  Action comparisonAction = Action::NoChange;
  Kind kind = Kind::File;
  bool included = false;
  bool destructive = false;
  bool syntheticDelete = false;
  std::optional<std::int64_t> size;

  QString identity() const;
};

struct PlanSummary {
  int included = 0;
  int excluded = 0;
  int modified = 0;
  int retainedDestinationOnly = 0;
  int destructive = 0;
  int creates = 0;
  int updates = 0;
  int metadata = 0;
  int deletions = 0;
  std::int64_t deletedSize = 0;
  std::int64_t totalSize = 0;
  std::int64_t includedSize = 0;
  bool backupEnabled = false;
};

struct ExecutionPlan {
  QVector<PlanItem> items;
  PlanSummary summary;
  QString itemsFingerprint;
  QString decisionSignature;
  QString profileFingerprint;
  QString comparisonSignature;
  QString sourceOperand;
  QString destinationOperand;

  [[nodiscard]] QVector<PlanItem> includedItems() const;
};

struct PlanMismatch {
  QStringList missing;
  QStringList unexpected;

  [[nodiscard]] bool valid() const {
    return missing.isEmpty() && unexpected.isEmpty();
  }
};

struct PlanFreshness {
  bool fresh = false;
  QStringList reasons;
};

using DestinationMetadata =
    QHash<QString, QPair<std::optional<std::int64_t>, QString>>;

QString actionName(Action action);
QString kindName(Kind kind);
QString planItemsFingerprint(const QVector<PlanItem>& items);
QString decisionSignature(const Decisions& decisions);
QString comparisonSignature(const QVector<Change>& changes,
                            const QSet<QString>& extraPaths = {});

ExecutionPlan buildExecutionPlan(const SyncProfile& profile,
                                 const QVector<Change>& changes,
                                 const Decisions& decisions,
                                 const DestinationMetadata& metadata = {},
                                 bool computeSignatures = true);
// Counts only, without materializing half a million PlanItems. Used while
// editing decisions; the full plan is rebuilt before it is executed.
PlanSummary buildPlanSummary(const SyncProfile& profile,
                             const QVector<Change>& changes,
                             const Decisions& decisions);
QStringList decisionFilterLines(const ExecutionPlan& plan);
QString decisionFilterText(const ExecutionPlan& plan);
PlanMismatch comparePlanToChanges(const ExecutionPlan& plan,
                                  const QVector<Change>& changes,
                                  const Decisions* decisions = nullptr);
PlanFreshness checkPlanFreshness(const ExecutionPlan& plan,
                                 const SyncProfile& profile,
                                 const QVector<Change>& changes,
                                 const QString& signature = {});

}  // namespace qt_rsync::core
