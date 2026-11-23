#include <QtTest>

#include "core/Change.h"
#include "core/Decision.h"
#include "core/Profile.h"

using namespace qt_rsync::core;

namespace {

Change makeChange(const QString& path, Action action,
                  Kind kind = Kind::File, int size = 0) {
  Change change;
  change.path = path;
  change.action = action;
  change.kind = kind;
  change.size = size;
  return change;
}

}  // namespace

class CoreTest : public QObject {
  Q_OBJECT

 private slots:
  // Decisions (ported from qpy-rsync tests/unit/test_changes.py)
  void changeProperties();
  void inheritedExclusionAndChildOverride();
  void nonSelectableRecordsAreExcluded();
  void reconcileDiscardsChangedIdentity();
  void summaryCountsIncludedExcludedAndDestructive();
  void destinationOnlyDefaultsExcludedButCanBeSelected();
  void reconciliationIsTypeAware();
  void preservationKeepsExcludeButNotIncludeOnDestinationOnly();
  void freshComparisonDropsStaleDecisionsWithoutConflicts();
  void inheritedDestinationOnlyDetectionIsOptIn();
  void deleteDecisionSurvivesPreserveWhileItemExists();

  // Existing smoke checks
  void endpointOperandKeepsSshSyntax();
};

void CoreTest::changeProperties() {
  QVERIFY(makeChange("old", Action::Delete).isDeletion());
  const Change destinationOnly = makeChange("extra", Action::DestinationOnly);
  QVERIFY(destinationOnly.isDestinationOnly());
  QVERIFY(!destinationOnly.isDeletion());
}

void CoreTest::inheritedExclusionAndChildOverride() {
  const QVector<Change> changes{makeChange("docs/a.txt", Action::Create)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("docs", Decision::Exclude);
  QCOMPARE(decisions.effective(changes.first()), Decision::Exclude);
  decisions.set("docs/a.txt", Decision::Include);
  QCOMPARE(decisions.effective(changes.first()), Decision::Include);
  decisions.set("docs/a.txt", Decision::Inherit);
  QCOMPARE(decisions.effective(changes.first()), Decision::Exclude);
}

void CoreTest::nonSelectableRecordsAreExcluded() {
  for (Action action : {Action::Message, Action::NoChange}) {
    const Change change = makeChange("x", action);
    QVERIFY(!Decisions::selectable(change));
    Decisions decisions;
    QCOMPARE(decisions.effective(change), Decision::Exclude);
  }
}

void CoreTest::reconcileDiscardsChangedIdentity() {
  QVector<Change> changes{makeChange("a.txt", Action::UpdateContent)};
  Decisions decisions;
  QCOMPARE(decisions.reconcile(changes, false), 0);
  decisions.set("a.txt", Decision::Exclude);
  QCOMPARE(decisions.reconcile(changes, false), 0);
  changes[0].action = Action::Delete;
  QCOMPARE(decisions.reconcile(changes, false), 1);
  QCOMPARE(decisions.explicitDecision("a.txt"), Decision::Inherit);
}

void CoreTest::summaryCountsIncludedExcludedAndDestructive() {
  const QVector<Change> changes{
      makeChange("new", Action::Create),
      makeChange("old", Action::Delete),
      makeChange("skip", Action::UpdateContent),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("skip", Decision::Exclude);
  const auto summary = decisions.summary(changes);
  QCOMPARE(summary.included, 2);
  QCOMPARE(summary.excluded, 1);
  QCOMPARE(summary.destructive, 1);
  QCOMPARE(summary.modified, 1);
}

void CoreTest::destinationOnlyDefaultsExcludedButCanBeSelected() {
  const QVector<Change> changes{makeChange("kept", Action::DestinationOnly)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  QVERIFY(Decisions::selectable(changes.first()));
  QCOMPARE(decisions.effective(changes.first()), Decision::Exclude);
  decisions.set("kept", Decision::Include);
  QCOMPARE(decisions.effective(changes.first()), Decision::Include);
  decisions.reset("kept");
  QCOMPARE(decisions.effective(changes.first()), Decision::Exclude);
}

void CoreTest::reconciliationIsTypeAware() {
  QVector<Change> changes{makeChange("a.txt", Action::Create)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("a.txt", Decision::Exclude);
  changes[0].action = Action::Delete;
  QCOMPARE(decisions.reconcile(changes, false), 1);
  QCOMPARE(decisions.explicitDecision("a.txt"), Decision::Inherit);

  QVector<Change> preservedChanges{makeChange("a.txt", Action::Create)};
  Decisions preserved;
  preserved.reconcile(preservedChanges, false);
  preserved.set("a.txt", Decision::Exclude);
  preservedChanges[0].action = Action::Delete;
  QCOMPARE(preserved.reconcile(preservedChanges, true), 0);
  QCOMPARE(preserved.explicitDecision("a.txt"), Decision::Exclude);
}

void CoreTest::preservationKeepsExcludeButNotIncludeOnDestinationOnly() {
  const QVector<Change> before{makeChange("a.txt", Action::UpdateContent)};
  const QVector<Change> after{makeChange("a.txt", Action::DestinationOnly)};

  Decisions excluded;
  excluded.reconcile(before, false);
  excluded.set("a.txt", Decision::Exclude);
  QCOMPARE(excluded.reconcile(after, true), 0);
  QVERIFY(excluded.conflicts().isEmpty());

  Decisions included;
  included.reconcile(before, false);
  included.set("a.txt", Decision::Include);
  QCOMPARE(included.reconcile(after, true), 1);
  QCOMPARE(included.explicitDecision("a.txt"), Decision::Inherit);
  QVERIFY(included.conflicted("a.txt"));
  QVERIFY(included.conflicts().value("a.txt").contains("destination-only"));
  QCOMPARE(included.acceptDefaultsForConflicts(), 1);
  QVERIFY(included.conflicts().isEmpty());
}

void CoreTest::freshComparisonDropsStaleDecisionsWithoutConflicts() {
  QVector<Change> changes{makeChange("a.txt", Action::Create)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("a.txt", Decision::Exclude);
  changes[0].action = Action::UpdateContent;
  QCOMPARE(decisions.reconcile(changes, false), 1);
  QCOMPARE(decisions.explicitDecision("a.txt"), Decision::Inherit);
  QVERIFY(decisions.conflicts().isEmpty());
}

void CoreTest::inheritedDestinationOnlyDetectionIsOptIn() {
  const QVector<Change> before{
      makeChange("docs", Action::UpdateMetadata, Kind::Directory),
      makeChange("docs/b.txt", Action::Create),
  };
  QVector<Change> after = before;
  after[1].action = Action::DestinationOnly;

  Decisions plain;
  plain.reconcile(before, false);
  plain.set("docs", Decision::Include);
  QCOMPARE(plain.reconcile(after, true), 0);
  QVERIFY(plain.conflicts().isEmpty());

  Decisions detecting;
  detecting.reconcile(before, false);
  detecting.set("docs", Decision::Include);
  QCOMPARE(detecting.reconcile(after, true, true), 1);
  QCOMPARE(detecting.explicitDecision("docs"), Decision::Inherit);
  QVERIFY(detecting.conflicts().value("docs").contains("destination-only"));
}

void CoreTest::deleteDecisionSurvivesPreserveWhileItemExists() {
  const QVector<Change> changes{makeChange(".parentlock", Action::NoChange)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set(".parentlock", Decision::Delete);
  QCOMPARE(decisions.reconcile(changes, true), 0);
  QCOMPARE(decisions.effective(changes.first()), Decision::Delete);
  QVERIFY(decisions.isSelectable(changes.first()));
  QCOMPARE(decisions.reconcile(changes, false), 1);
  QCOMPARE(decisions.effective(changes.first()), Decision::Exclude);
}

void CoreTest::endpointOperandKeepsSshSyntax() {
  Endpoint endpoint;
  endpoint.kind = EndpointKind::Ssh;
  endpoint.host = QStringLiteral("example");
  endpoint.path = QStringLiteral("/home/user/src");
  QCOMPARE(endpoint.rsyncOperand(), QStringLiteral("example:/home/user/src"));
  endpoint.user = QStringLiteral("user");
  QCOMPARE(endpoint.rsyncOperand(), QStringLiteral("user@example:/home/user/src"));
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
