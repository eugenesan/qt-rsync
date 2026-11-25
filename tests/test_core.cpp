#include <QtTest>

#include "core/Change.h"
#include "core/Decision.h"
#include "core/Filter.h"
#include "core/Plan.h"
#include "core/Command.h"
#include "core/Storage.h"
#include "core/Parser.h"
#include "core/ExternalTools.h"
#include "core/Paths.h"

#include <QTemporaryDir>
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
  void rsyncProgressDerivedFromCounters();
  void externalToolPlaceholders();
  void liveRecordCheckMatchesParser();
  void quickPlanSummaryMatchesFullPlan();
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

  // rsync output parser
  void parseItemizedRecords();

  // Storage (profiles, settings split, locks)
  void profileJsonRoundTrip();
  void legacyProfileFieldsMigrate();
  void settingsAreSplitByPrefix();
  void pidLocksDetectLiveOwnersAndTakeOverStaleOnes();
  void legacyPathsAreMigrated();
  void sshEndpointTextIsParsed();
  void planMismatchReportsMissingAndUnexpected();

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
  // qpy's rsync pattern oracle: a basename matches at any depth, a leading
  // slash anchors to the compared folder, and a trailing slash covers a tree.
  QVERIFY(patternMatches(QStringLiteral("*.jpg"), QStringLiteral("pictures/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("photo.jpg"), QStringLiteral("a/b/photo.jpg")));
  QVERIFY(!patternMatches(QStringLiteral("*.jpg"), QStringLiteral("pictures/photo.png")));
  QVERIFY(patternMatches(QStringLiteral("pictures/"), QStringLiteral("a/b/pictures/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("/docs/"), QStringLiteral("docs/pictures/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("/docs/pictures/"), QStringLiteral("docs/pictures/photo.jpg")));
  QVERIFY(!patternMatches(QStringLiteral("/docs/pictures/photo.jpg"),
                          QStringLiteral("other/docs/pictures/photo.jpg")));
  QVERIFY(patternMatches(QStringLiteral("[0-9]*.txt"), QStringLiteral("2024.txt")));
  QVERIFY(!patternMatches(QStringLiteral("[0-9]*.txt"), QStringLiteral("docs/report.txt")));
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
  QVERIFY(staleChanges.reasons.contains("comparison decisions changed"));
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
      "comparison decisions changed"));
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

void CoreTest::parseItemizedRecords() {
  const QString output = QStringLiteral(
      ">f+++++++++|qpy-rsync|123|qpy-rsync|2026-11-23|qpy-rsync|docs/a.txt\n"
      "*deleting|qpy-rsync|0|qpy-rsync||qpy-rsync|old.txt\n"
      ".d..t......|qpy-rsync|0|qpy-rsync||qpy-rsync|docs/\n"
      "random progress output\n");
  const ParseResult result = parseOutput(output);
  QCOMPARE(result.changes.size(), 3);
  QCOMPARE(result.changes.at(0).path, QStringLiteral("docs/a.txt"));
  QCOMPARE(result.changes.at(0).action, Action::Create);
  QCOMPARE(result.changes.at(0).size.value(), 123);
  QCOMPARE(result.changes.at(1).action, Action::Delete);
  QCOMPARE(result.changes.at(2).action, Action::UpdateMetadata);
  QCOMPARE(result.diagnostics.size(), 1);
  QVERIFY(result.diagnostics.first().contains(QStringLiteral("progress")));

  const ParseResult retained = parseOutput(output, false);
  QCOMPARE(retained.changes.at(1).action, Action::DestinationOnly);
}

void CoreTest::profileJsonRoundTrip() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  SyncProfile profile = testProfile();
  profile.id = QStringLiteral("abc");
  profile.name = QStringLiteral("example");
  profile.transfer.mode = SyncMode::Mirror;
  profile.transfer.deleteTiming = DeleteTiming::After;
  profile.backup.directory = QStringLiteral("/dst/.backup");
  profile.filterText = QStringLiteral("*.tmp\n-s .parentlock");

  const QString path = dir.path() + QStringLiteral("/profile.json");
  storage::saveProfile(path, profile);
  const SyncProfile loaded = storage::loadProfile(path);
  QCOMPARE(loaded.name, profile.name);
  QCOMPARE(loaded.id, profile.id);
  QCOMPARE(loaded.transfer.mode, SyncMode::Mirror);
  QCOMPARE(loaded.transfer.deleteTiming, DeleteTiming::After);
  QCOMPARE(loaded.backup.directory, QStringLiteral("/dst/.backup"));
  QCOMPARE(loaded.filterText, profile.filterText);
  QCOMPARE(loaded.filters.size(), 2);
  QCOMPARE(loaded.fingerprint(), profile.fingerprint());
}

void CoreTest::legacyProfileFieldsMigrate() {
  QTemporaryDir dir;
  const QString payload = QStringLiteral(R"({
    "name": "legacy",
    "source": {"kind": "local", "path": "/src/"},
    "destination": {"kind": "local", "path": "/dst/"},
    "transfer": {"delete": true, "delete_timing": "after"},
    "backup": {"mode": "none", "directory": "/backup"}
  })");
  const SyncProfile profile = storage::profileFromJson(payload);
  QCOMPARE(profile.transfer.mode, SyncMode::Mirror);
  QCOMPARE(profile.transfer.deleteTiming, DeleteTiming::After);
  QVERIFY(profile.backup.directory.isEmpty());
}

void CoreTest::settingsAreSplitByPrefix() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  storage::Settings settings;
  settings.setValue(QStringLiteral("logging/timing"), true);
  settings.setValue(QStringLiteral("decisions/preserve"), true);
  settings.setValue(QStringLiteral("operation/clock"), 42);
  settings.sync();
  QVERIFY(settings.value(QStringLiteral("logging/timing")).toBool());
  QCOMPARE(settings.value(QStringLiteral("operation/clock")).toInt(), 42);
  QVERIFY(QFile::exists(settings.statePath()));
  QVERIFY(QFile::exists(settings.configPath()));
  QVERIFY(settings.configPath() != settings.statePath());

  storage::setCurrentProfilePath(QStringLiteral("/tmp/profile.json"));
  QCOMPARE(storage::currentProfilePath(), QStringLiteral("/tmp/profile.json"));
}

void CoreTest::pidLocksDetectLiveOwnersAndTakeOverStaleOnes() {
  QTemporaryDir dir;
  const QString path = dir.path() + QStringLiteral("/test.json");

  storage::FileLock first(path);
  QVERIFY(first.acquire());
  QCOMPARE(storage::readLockPid(path), static_cast<qint64>(::getpid()));
  first.release();
  QVERIFY(!QFile::exists(first.lockPath()));

  // PID 1 is always alive: a live foreign lock must block.
  QFile live(first.lockPath());
  QVERIFY(live.open(QIODevice::WriteOnly | QIODevice::Truncate));
  live.write("1");
  live.close();
  storage::FileLock blocked(path, 0);
  QVERIFY(!blocked.acquire());

  // A dead PID is replaced silently.
  QFile stale(first.lockPath());
  QVERIFY(stale.open(QIODevice::WriteOnly | QIODevice::Truncate));
  stale.write("999999");
  stale.close();
  storage::FileLock takeover(path, 0);
  QVERIFY(takeover.acquire());
  takeover.release();
  QVERIFY(!QFile::exists(first.lockPath()));
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

void CoreTest::legacyPathsAreMigrated() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());

  const QString oldConfig = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME")) +
                            QStringLiteral("/qpy-rsync");
  const QString oldProfiles = oldConfig + QStringLiteral("/profiles");
  QDir().mkpath(oldProfiles);
  const QString profile = oldProfiles + QStringLiteral("/old.json");
  SyncProfile original;
  original.name = QStringLiteral("old");
  original.source.path = QStringLiteral("/src");
  original.destination.path = QStringLiteral("/dst");
  storage::saveProfile(profile, original);
  QSettings oldSettings(oldConfig + QStringLiteral("/settingsrc"),
                        QSettings::IniFormat);
  oldSettings.setValue(QStringLiteral("profile/current_path"), profile);
  oldSettings.sync();

  storage::migrateLegacyPaths();
  QVERIFY(QFile::exists(qt_rsync::core::paths::profilesDirectory() + QStringLiteral("/old.json")));
  QVERIFY(QFile::exists(qt_rsync::core::paths::configSettingsPath()));
  QVERIFY(QFile::exists(qt_rsync::core::paths::logDirectory()));
  QVERIFY(storage::currentProfilePath().contains(QStringLiteral("/qt-rsync/profiles/")));
}

void CoreTest::sshEndpointTextIsParsed() {
  const Endpoint remote = parseEndpoint(QStringLiteral("alice@example.test:/home/user/src"));
  QCOMPARE(remote.kind, EndpointKind::Ssh);
  QCOMPARE(remote.user, QStringLiteral("alice"));
  QCOMPARE(remote.host, QStringLiteral("example.test"));
  QCOMPARE(remote.path, QStringLiteral("/home/user/src"));
  QCOMPARE(remote.rsyncOperand(), QStringLiteral("alice@example.test:/home/user/src"));
  const Endpoint local = parseEndpoint(QStringLiteral("/tmp/source"));
  QCOMPARE(local.kind, EndpointKind::Local);
  QCOMPARE(local.path, QStringLiteral("/tmp/source"));
}

void CoreTest::planMismatchReportsMissingAndUnexpected() {
  const QVector<Change> changes{
      makeChange("a.txt", Action::Create, Kind::File, 10),
      makeChange("b.txt", Action::UpdateContent),
  };
  Decisions decisions;
  decisions.reconcile(changes, false);
  const auto plan = buildExecutionPlan(testProfile(), changes, decisions);

  QVector<Change> verified = changes;
  const auto match = comparePlanToChanges(plan, verified, &decisions);
  QVERIFY(match.valid());

  verified.removeLast();
  verified.append(makeChange("extra.txt", Action::Create));
  const auto mismatch = comparePlanToChanges(plan, verified, &decisions);
  QVERIFY(!mismatch.valid());
  QVERIFY(mismatch.missing.contains([&] {
    return QStringLiteral("b.txt") + QChar('\x1f') +
           actionName(Action::UpdateContent) + QChar('\x1f') + kindName(Kind::File);
  }()));
  QCOMPARE(mismatch.unexpected.size(), 1);
}

void CoreTest::rsyncProgressDerivedFromCounters() {
  const auto progress = parseSyncProgress(
      QStringLiteral("  8 100%    0.00kB/s    0:00:00 (xfr#2, to-chk=3/10)"));
  QVERIFY(progress.has_value());
  QCOMPARE(progress->filesCompleted, 2);
  QCOMPARE(progress->filesRemaining, 3);
  QCOMPARE(progress->filesTotal, 10);
  QCOMPARE(progress->percent, 70);
  QVERIFY(!parseSyncProgress(QStringLiteral(">f+++++++++ file.txt")).has_value());
  QVERIFY(isRsyncSummaryLine(QStringLiteral("Number of files: 10")));
  QVERIFY(!isRsyncSummaryLine(
      QStringLiteral(">f+++++++++ file.txt")));
}

namespace {
QStringList toolArgs(const QString& a, const QString& b = QString(),
                     const QString& c = QString(), const QString& d = QString()) {
  QStringList result;
  result << a;
  if (!b.isEmpty()) result << b;
  if (!c.isEmpty()) result << c;
  if (!d.isEmpty()) result << d;
  return result;
}
}  // namespace

void CoreTest::externalToolPlaceholders() {
  Endpoint localSource{EndpointKind::Local, QStringLiteral("/src")};
  Endpoint localDestination{EndpointKind::Local, QStringLiteral("/dst")};
  Endpoint remoteDestination{EndpointKind::Ssh, QStringLiteral("/dst")};
  remoteDestination.host = QStringLiteral("127.0.0.1");
  remoteDestination.user = QStringLiteral("me");

  const auto context = [&](const Endpoint& other,
                           const QString& path = QStringLiteral("docs/photo.png"),
                           bool isDirectory = false) {
    return itemContext(path, isDirectory, QStringLiteral("/src"),
                       QStringLiteral("/dst"), localSource, other,
                       QStringLiteral("Kate"));
  };
  const auto run = [&](const QString& command, const ToolContext& toolContext,
                       const QString& options = QString()) -> QStringList {
    auto values = placeholderValues(toolContext);
    values.insert(QStringLiteral("O"), options);
    return buildToolCommand(command, values);
  };

  const auto local = context(localDestination);
  QCOMPARE(run(QStringLiteral("kate %d/%f"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("/src/docs/photo.png")));
  QCOMPARE(run(QStringLiteral("kate %a"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("/src/docs/photo.png")));
  QCOMPARE(run(QStringLiteral("kate %f"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("photo.png")));
  QCOMPARE(run(QStringLiteral("kate %x"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("png")));
  QCOMPARE(run(QStringLiteral("kate %b"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("photo")));
  QCOMPARE(run(QStringLiteral("kate %d"), local),
           toolArgs(QStringLiteral("kate"), QStringLiteral("/src/docs")));
  QCOMPARE(run(QStringLiteral("meld %A"), local),
           toolArgs(QStringLiteral("meld"), QStringLiteral("/dst/docs/photo.png")));
  QCOMPARE(run(QStringLiteral("meld %D"), local),
           toolArgs(QStringLiteral("meld"), QStringLiteral("/dst/docs")));
  QCOMPARE(run(QStringLiteral("meld %T"), local),
           toolArgs(QStringLiteral("meld"), QStringLiteral("Kate")));
  QCOMPARE(run(QStringLiteral("kate %O %f"), local, QStringLiteral("-R --wait")),
           toolArgs(QStringLiteral("kate"), QStringLiteral("-R"),
                    QStringLiteral("--wait"), QStringLiteral("photo.png")));

  const auto remote = context(remoteDestination);
  QCOMPARE(run(QStringLiteral("scp %d/%f %U%V%R%W%D/%F"), remote),
           toolArgs(QStringLiteral("scp"), QStringLiteral("/src/docs/photo.png"),
                    QStringLiteral("me@127.0.0.1:/dst/docs/photo.png")));
  QCOMPARE(run(QStringLiteral("kate %P"), remote),
           toolArgs(QStringLiteral("kate"), QStringLiteral("://")));
  QCOMPARE(run(QStringLiteral("kate %R"), local), toolArgs(QStringLiteral("kate")));
  QCOMPARE(run(QStringLiteral("echo %%"), local),
           toolArgs(QStringLiteral("echo"), QStringLiteral("%")));
  QCOMPARE(run(QStringLiteral("kate %a"),
               context(localDestination, QStringLiteral("my docs/photo.png"))),
           toolArgs(QStringLiteral("kate"), QStringLiteral("/src/my docs/photo.png")));
  QCOMPARE(run(QStringLiteral("meld %D %d"),
               context(localDestination, QStringLiteral("docs"), true)),
           toolArgs(QStringLiteral("meld"), QStringLiteral("/dst/docs"),
                    QStringLiteral("/src/docs")));

  QCOMPARE(unknownPlaceholders(QStringLiteral("kate %z %f")),
           toolArgs(QStringLiteral("%z")));
  QVERIFY(unknownPlaceholders(QStringLiteral("kate %f %d")).isEmpty());
  QString emptyError;
  QVERIFY(buildToolCommand(QString(), placeholderValues(local), &emptyError).isEmpty());
  QVERIFY(!emptyError.isEmpty());
}

void CoreTest::liveRecordCheckMatchesParser() {
  const QString record = QStringLiteral(
      ">f+++++++++|qpy-rsync|1|qpy-rsync|2026-11-23|qpy-rsync|docs/file.txt");
  QVERIFY(looksLikeItemizedRecord(record));
  QVERIFY(parseRecord(record).has_value());
  QVERIFY(!looksLikeItemizedRecord(QStringLiteral("a|qpy-rsync|b")));
  QVERIFY(!looksLikeItemizedRecord(QStringLiteral("checking to-chk=1/2")));
  QVERIFY(!looksLikeItemizedRecord(
      QStringLiteral(">f+++++++++|qpy-rsync|1|qpy-rsync|t|qpy-rsync|.")));
  const QString deletion = QStringLiteral(
      "*deleting   |qpy-rsync|0|qpy-rsync|2026-11-23|qpy-rsync|docs/old/");
  QVERIFY(looksLikeItemizedRecord(deletion));
  QVERIFY(parseRecord(deletion).has_value());
}

void CoreTest::quickPlanSummaryMatchesFullPlan() {
  const QVector<Change> changes{
      makeChange("new.txt", Action::Create, Kind::File, 100),
      makeChange("old.txt", Action::UpdateMetadata, Kind::File, 0),
      makeChange("gone.txt", Action::Delete, Kind::File, 20),
      makeChange("dest.txt", Action::DestinationOnly, Kind::File, 30),
      makeChange("skip.txt", Action::Create, Kind::File, 40),
  };
  Decisions decisions;
  decisions.set(QStringLiteral("skip.txt"), Decision::Exclude);
  decisions.set(QStringLiteral("gone.txt"), Decision::Include);

  const auto full = buildExecutionPlan(testProfile(), changes, decisions);
  const auto quick = buildPlanSummary(testProfile(), changes, decisions);
  QCOMPARE(quick.included, full.summary.included);
  QCOMPARE(quick.excluded, full.summary.excluded);
  QCOMPARE(quick.creates, full.summary.creates);
  QCOMPARE(quick.updates, full.summary.updates);
  QCOMPARE(quick.metadata, full.summary.metadata);
  QCOMPARE(quick.deletions, full.summary.deletions);
  QCOMPARE(quick.destructive, full.summary.destructive);
  QCOMPARE(quick.includedSize, full.summary.includedSize);
  QCOMPARE(quick.totalSize, full.summary.totalSize);
  QCOMPARE(quick.modified, full.summary.modified);
  QCOMPARE(quick.retainedDestinationOnly, full.summary.retainedDestinationOnly);
  QCOMPARE(quick.backupEnabled, full.summary.backupEnabled);
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
