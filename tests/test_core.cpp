#include <QtTest>

#include "core/Change.h"
#include "core/Decision.h"
#include "core/Filter.h"
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

  // Filters (ported from qpy-rsync filter tests)
  void advancedRuleParsing();
  void filterTextConvertsBarePatterns();
  void filterArgumentsPreserveOrderAndTypes();
  void contextMenuPhrases();
  void patternMatching();
  void cleanQtNoiseRemovesOnlyBenignLines();

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

void CoreTest::advancedRuleParsing() {
  QVERIFY(isAdvancedRule(QStringLiteral("-s *.kate-swp")));
  QVERIFY(isAdvancedRule(QStringLiteral("exclude *.tmp")));
  QVERIFY(isAdvancedRule(QStringLiteral("clear")));
  QVERIFY(!isAdvancedRule(QStringLiteral("-z foo")));
  QVERIFY(!isAdvancedRule(QStringLiteral("-s")));
  QVERIFY(!isAdvancedRule(QStringLiteral("! keep.txt")));
  const auto parsed = parseAdvancedRule(QStringLiteral("-s *.kate-swp"));
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->token, QStringLiteral("-s"));
  QCOMPARE(parsed->pattern, QStringLiteral("*.kate-swp"));
}

void CoreTest::filterTextConvertsBarePatterns() {
  const auto rules = filterTextToRules(QStringLiteral(
      "# comment\n*.tmp\n-s .parentlock\n\ninclude /.git/"));
  QCOMPARE(rules.size(), 3);
  QCOMPARE(rules.at(0).rule, QStringLiteral("- *.tmp"));
  QCOMPARE(rules.at(1).rule, QStringLiteral("-s .parentlock"));
  QCOMPARE(rules.at(2).rule, QStringLiteral("include /.git/"));
  for (const auto& rule : rules) QCOMPARE(rule.type, RuleType::Advanced);
}

void CoreTest::filterArgumentsPreserveOrderAndTypes() {
  QVector<FilterRule> rules{
      FilterRule{QStringLiteral("*.txt"), RuleType::Include},
      FilterRule{QStringLiteral("- *.tmp"), RuleType::Advanced},
      FilterRule{QStringLiteral("skip"), RuleType::Exclude},
  };
  rules.append(FilterRule{QStringLiteral("off"), RuleType::Exclude, false});
  QCOMPARE(filterArguments(rules), QStringList({
      QStringLiteral("--include=*.txt"),
      QStringLiteral("--filter"),
      QStringLiteral("- *.tmp"),
      QStringLiteral("--exclude=skip"),
  }));
}

void CoreTest::contextMenuPhrases() {
  const auto filePhrases = phrasesFor(QStringLiteral("docs/pictures/photo.jpg"), false);
  QStringList patterns;
  for (const auto& phrase : filePhrases) patterns << phrase.pattern;
  QCOMPARE(patterns, QStringList({QStringLiteral("*.jpg"),
                                  QStringLiteral("photo.jpg"),
                                  QStringLiteral("/docs/pictures/photo.jpg")}));
  const auto dirPhrases = phrasesFor(QStringLiteral("docs/pictures"), true);
  QStringList dirPatterns;
  for (const auto& phrase : dirPhrases) dirPatterns << phrase.pattern;
  QCOMPARE(dirPatterns, QStringList({QStringLiteral("pictures/"),
                                     QStringLiteral("/docs/pictures/")}));
  const auto deletePhrases = deletePhrasesFor(QStringLiteral(".parentlock"), false);
  QStringList deletePatterns;
  for (const auto& phrase : deletePhrases) deletePatterns << phrase.pattern;
  QCOMPARE(deletePatterns, QStringList({QStringLiteral("-s *.parentlock"),
                                        QStringLiteral("-s .parentlock"),
                                        QStringLiteral("-s /.parentlock")}));
}

void CoreTest::patternMatching() {
  QVERIFY(patternMatches(QStringLiteral("*.jpg"), QStringLiteral("docs/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("photo.jpg"), QStringLiteral("photo.jpg")));
  QVERIFY(!patternMatches(QStringLiteral("/docs/pictures/photo.jpg"),
                          QStringLiteral("other/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("docs/"), QStringLiteral("docs/a/b.txt")));
  QVERIFY(patternMatches(QStringLiteral("a/**/x.txt"), QStringLiteral("a/b/c/x.txt")));
  QVERIFY(!patternMatches(QString(), QStringLiteral("a.txt")));
}

void CoreTest::cleanQtNoiseRemovesOnlyBenignLines() {
  const QString raw = QStringLiteral(
      "QXcbIntegration: Cannot create platform OpenGL context\n"
      "Warning: fallback to QtQuick software backend.\n"
      "Permission denied, please try again.");
  QCOMPARE(cleanQtNoise(raw), QStringLiteral("Permission denied, please try again."));
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
