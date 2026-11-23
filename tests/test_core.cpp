#include <QtTest>

#include "core/Change.h"
#include "core/Decision.h"
#include "core/Filter.h"
#include "core/Plan.h"
#include "core/Command.h"
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

  // Execution plan (ported from qpy-rsync plan/decision-filter tests)
  void planSummaryAndCounts();
  void destinationOnlyIsNotExecutableByDefault();
  void planFreshnessDetectsChanges();
  void profileFingerprintIgnoresIdentity();
  void selectedDestinationOnlyBecomesDelete();
  void includedSizeCountsOnlyTransferredData();
  void deletedSizeUsesMetadataWithoutDoubleCounting();
  void syntheticDeleteEntersPlan();
  void decisionFilterRulesAndOrdering();
  void retainedDestinationOnlyIsProtected();
  void planFreshnessAcceptsSyntheticDelete();

  // rsync command construction
  void compareSpecUsesDryRunAndDestinationInventory();
  void syncSpecIsWriteCapableAndHasNoDryRun();
  void filterMergePrecedesUserFilters();
  void mirrorAndFuzzyChangeDeleteTiming();
  void nestedBackupAddsProtectionRules();
  void sshEndpointsUseSetsidAndRemoteShell();

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

namespace {

SyncProfile testProfile() {
  SyncProfile profile;
  profile.name = QStringLiteral("plan");
  profile.source.path = QStringLiteral("/src/");
  profile.destination.path = QStringLiteral("/dst/");
  return profile;
}

}  // namespace

void CoreTest::planSummaryAndCounts() {
  const QVector<Change> changes{
      makeChange("new", Action::Create, Kind::File, 10),
      makeChange("old", Action::Delete),
      makeChange("skip", Action::UpdateContent),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("skip", Decision::Exclude);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(plan.summary.included, 2);
  QCOMPARE(plan.summary.excluded, 1);
  QCOMPARE(plan.summary.destructive, 1);
  QCOMPARE(plan.summary.creates, 1);
  QCOMPARE(plan.summary.deletions, 1);
}

void CoreTest::destinationOnlyIsNotExecutableByDefault() {
  const QVector<Change> changes{makeChange("kept", Action::DestinationOnly)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(plan.summary.included, 0);
  QCOMPARE(plan.summary.excluded, 0);
  QCOMPARE(plan.summary.retainedDestinationOnly, 1);
}

void CoreTest::planFreshnessDetectsChanges() {
  const QVector<Change> changes{makeChange("a", Action::Create)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  SyncProfile profile = testProfile();
  const auto plan = buildExecutionPlan(profile, changes, decisions);
  QVERIFY(checkPlanFreshness(plan, profile, changes).fresh);

  profile.backup.directory = QStringLiteral("/recovery");
  const auto staleProfile = checkPlanFreshness(plan, profile, changes);
  QVERIFY(!staleProfile.fresh);
  QVERIFY(staleProfile.reasons.contains("profile options changed"));

  QVector<Change> changed{makeChange("a", Action::UpdateContent)};
  const auto staleChanges = checkPlanFreshness(plan, testProfile(), changed);
  QVERIFY(!staleChanges.fresh);
  QVERIFY(staleChanges.reasons.contains("comparison actions changed"));
}

void CoreTest::profileFingerprintIgnoresIdentity() {
  SyncProfile first = testProfile();
  first.id = QStringLiteral("first");
  SyncProfile second = testProfile();
  second.id = QStringLiteral("second");
  QCOMPARE(first.fingerprint(), second.fingerprint());
  second.transfer.legacyDelete = true;
  QVERIFY(first.fingerprint() != second.fingerprint());
}

void CoreTest::selectedDestinationOnlyBecomesDelete() {
  const QVector<Change> changes{makeChange("old", Action::DestinationOnly)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("old", Decision::Include);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(plan.summary.included, 1);
  QCOMPARE(plan.summary.retainedDestinationOnly, 0);
  QCOMPARE(plan.summary.deletions, 1);
  QCOMPARE(plan.items.first().action, Action::Delete);
  QCOMPARE(plan.items.first().comparisonAction, Action::DestinationOnly);
  QVERIFY(plan.items.first().destructive);
}

void CoreTest::includedSizeCountsOnlyTransferredData() {
  const QVector<Change> changes{
      makeChange("a.bin", Action::Create, Kind::File, 1000),
      makeChange("b.bin", Action::Delete, Kind::File, 2000),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("b.bin", Decision::Exclude);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(plan.summary.included, 1);
  QCOMPARE(plan.summary.totalSize, 1000);
}

void CoreTest::deletedSizeUsesMetadataWithoutDoubleCounting() {
  const QVector<Change> changes{
      makeChange("old", Action::Delete, Kind::Directory),
      makeChange("old/a", Action::Delete, Kind::File),
      makeChange("old/b", Action::Delete, Kind::File),
      makeChange("lone.txt", Action::Delete, Kind::File),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  DestinationMetadata metadata;
  metadata.insert(QStringLiteral("old"), {4096, QString()});
  metadata.insert(QStringLiteral("old/a"), {1000, QString()});
  metadata.insert(QStringLiteral("old/b"), {2000, QString()});
  metadata.insert(QStringLiteral("lone.txt"), {500, QString()});
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions, metadata);
  QCOMPARE(plan.summary.deletedSize, 3500);
}

void CoreTest::syntheticDeleteEntersPlan() {
  const QVector<Change> changes{makeChange(".parentlock", Action::NoChange)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set(".parentlock", Decision::Delete);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(plan.items.size(), 1);
  QVERIFY(plan.items.first().included);
  QVERIFY(plan.items.first().syntheticDelete);
  QCOMPARE(plan.items.first().comparisonAction, Action::NoChange);
  QCOMPARE(plan.summary.deletions, 1);
  QCOMPARE(decisionFilterLines(plan),
           QStringList({QStringLiteral("-s /.parentlock")}));
  QVERIFY(!checkPlanFreshness(plan, testProfile(), changes,
                              decisionSignature(decisions)).reasons.contains(
      "comparison actions changed"));
}

void CoreTest::decisionFilterRulesAndOrdering() {
  const QVector<Change> changes{
      makeChange("docs", Action::UpdateMetadata, Kind::Directory),
      makeChange("old.txt", Action::Delete),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set("docs", Decision::Exclude);
  decisions.set("old.txt", Decision::Exclude);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(decisionFilterLines(plan), QStringList({
      QStringLiteral("- /docs/***"),
      QStringLiteral("P /old.txt"),
      QStringLiteral("- /old.txt"),
  }));
}

void CoreTest::retainedDestinationOnlyIsProtected() {
  const QVector<Change> changes{makeChange("extra.txt", Action::DestinationOnly)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  const auto retained = buildExecutionPlan(testProfile(), changes, decisions);
  QCOMPARE(decisionFilterLines(retained), QStringList({
      QStringLiteral("P /extra.txt"),
      QStringLiteral("- /extra.txt"),
  }));
  decisions.set("extra.txt", Decision::Include);
  const auto selected = buildExecutionPlan(testProfile(), changes, decisions);
  QVERIFY(decisionFilterLines(selected).isEmpty());
}

void CoreTest::planFreshnessAcceptsSyntheticDelete() {
  const QVector<Change> changes{makeChange(".parentlock", Action::NoChange)};
  Decisions decisions;
  decisions.reconcile(changes, false);
  decisions.set(".parentlock", Decision::Delete);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);
  QVERIFY(checkPlanFreshness(plan, testProfile(), changes,
                             decisionSignature(decisions)).fresh);
}

void CoreTest::compareSpecUsesDryRunAndDestinationInventory() {
  const auto spec = buildCompareSpec(testProfile());
  QCOMPARE(spec.program, QStringLiteral("rsync"));
  const QStringList args = spec.arguments;
  QVERIFY(args.contains(QStringLiteral("--dry-run")));
  QVERIFY(args.contains(QStringLiteral("--itemize-changes")));
  QVERIFY(args.contains(QStringLiteral("--archive")));
  QVERIFY(args.contains(QStringLiteral("--no-compress")));
  QVERIFY(args.contains(QStringLiteral("--delete-during")));
  QCOMPARE(args.last(), QStringLiteral("/dst/"));
  QCOMPARE(args.at(args.size() - 2), QStringLiteral("/src/"));
}

void CoreTest::syncSpecIsWriteCapableAndHasNoDryRun() {
  const auto spec = buildSyncSpec(testProfile());
  QVERIFY(!spec.arguments.contains(QStringLiteral("--dry-run")));
  QVERIFY(spec.arguments.contains(QStringLiteral("--stats")));
  QVERIFY(!spec.arguments.contains(QStringLiteral("--delete-during")));
  QVERIFY(spec.arguments.contains(QStringLiteral("--no-compress")));
}

void CoreTest::filterMergePrecedesUserFilters() {
  SyncProfile profile = testProfile();
  profile.filters = filterTextToRules(QStringLiteral("*.tmp"));
  const auto spec = buildSyncSpec(profile, QStringLiteral("/tmp/rules"));
  const QStringList args = spec.arguments;
  const int merge = args.indexOf(QStringLiteral("--filter=merge /tmp/rules"));
  const int user = args.indexOf(QStringLiteral("- *.tmp"));
  QVERIFY(merge >= 0);
  QVERIFY(user > merge);
}

void CoreTest::mirrorAndFuzzyChangeDeleteTiming() {
  SyncProfile profile = testProfile();
  profile.transfer.mode = SyncMode::Mirror;
  QVERIFY(buildCompareSpec(profile).arguments.contains(
      QStringLiteral("--delete-during")));
  profile.transfer.fuzzy = true;
  QVERIFY(buildCompareSpec(profile).arguments.contains(
      QStringLiteral("--delete-after")));
  QVERIFY(buildCompareSpec(profile).arguments.contains(QStringLiteral("--fuzzy")));
}

void CoreTest::nestedBackupAddsProtectionRules() {
  SyncProfile profile = testProfile();
  profile.backup.directory = QStringLiteral("/src/.backup");
  const QStringList args = buildSyncSpec(profile).arguments;
  QVERIFY(args.contains(QStringLiteral("--backup")));
  QVERIFY(args.contains(QStringLiteral("--backup-dir=/src/.backup")));
  QVERIFY(args.contains(QStringLiteral("P /.backup/***")));
  QVERIFY(args.contains(QStringLiteral("- /.backup/***")));
}

void CoreTest::sshEndpointsUseSetsidAndRemoteShell() {
  qputenv("DISPLAY", ":50");
  SyncProfile profile = testProfile();
  profile.destination.kind = EndpointKind::Ssh;
  profile.destination.host = QStringLiteral("example");
  profile.destination.path = QStringLiteral("/home/user/dst");
  const auto spec = buildSyncSpec(profile);
  QCOMPARE(spec.program, QStringLiteral("setsid"));
  QCOMPARE(spec.arguments.first(), QStringLiteral("--wait"));
  QCOMPARE(spec.arguments.at(1), QStringLiteral("rsync"));
  const QStringList args = spec.arguments;
  const int shell = args.indexOf(QStringLiteral("-e"));
  QVERIFY(shell >= 0);
  QCOMPARE(args.at(shell + 1), QStringLiteral("env DISPLAY=:50 ssh"));
  QCOMPARE(args.last(), QStringLiteral("example:/home/user/dst"));
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
