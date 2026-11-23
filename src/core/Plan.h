#pragma once

#include <QHash>
#include <QString>
#include <QVector>

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
};

ExecutionPlan buildExecutionPlan(const SyncProfile& profile,
                                 const QVector<Change>& changes,
                                 const Decisions& decisions);
QString decisionFilterText(const ExecutionPlan& plan);
bool isPlanFresh(const ExecutionPlan& plan, const SyncProfile& profile,
                 const QVector<Change>& changes,
                 const QString& decisionSignature);

}  // namespace qt_rsync::core
