#include "core/Plan.h"

namespace qt_rsync::core {

ExecutionPlan buildExecutionPlan(const SyncProfile&, const QVector<Change>&,
                                 const Decisions&) {
  return {};
}

QString decisionFilterText(const ExecutionPlan&) { return {}; }

bool isPlanFresh(const ExecutionPlan&, const SyncProfile&,
                 const QVector<Change>&, const QString&) {
  return false;
}

}  // namespace qt_rsync::core
