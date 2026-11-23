#include "core/Decision.h"

namespace qt_rsync::core {

void Decisions::set(const QString&, Decision) {}
void Decisions::reset(const QString&) {}
void Decisions::resetSubtree(const QString&) {}
void Decisions::clear() {}

Decision Decisions::explicitDecision(const QString&) const { return Decision::Inherit; }

Decision Decisions::effective(const Change& change) const {
  if (change.action == Action::NoChange || change.action == Action::Message) {
    return Decision::Exclude;
  }
  if (change.action == Action::DestinationOnly) return Decision::Exclude;
  return Decision::Include;
}

bool Decisions::isSelectable(const Change& change) const {
  return change.action != Action::NoChange && change.action != Action::Message;
}

QVector<QPair<QString, Decision>> Decisions::explicitItems() const { return {}; }
QSet<QString> Decisions::excludedPaths() const { return {}; }
DecisionSummary Decisions::summary(const QVector<Change>&) const { return {}; }
int Decisions::reconcile(const QVector<Change>&, bool) { return 0; }

}  // namespace qt_rsync::core
