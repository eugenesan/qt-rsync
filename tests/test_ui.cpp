#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QHoverEvent>
#include <QLabel>
#include <QAbstractButton>
#include <QPainter>
#include <QPixmap>
#include <QSplitter>
#include <QStyleOptionViewItem>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QTimer>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QTabWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCursor>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTableView>
#include <QTableWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMenuBar>
#include <QToolButton>
#include <QTreeView>
#include <QImage>
#include <QShortcut>

#include "ui/MainWindow.h"
#include "app/StartupOptions.h"
#include "app/Application.h"
#include "model/DataStore.h"
#include "ui/ComparisonView.h"
#include "ui/DecisionDelegate.h"
#include "ui/FilterEditorWidget.h"
#include "ui/NativeDelegate.h"
#include "ui/OverviewPane.h"
#include "ui/SettingsDialog.h"
#include "ui/ProfileEditorDialog.h"
#include "ui/SyncDialog.h"
#include "ui/UseAsLocationDialog.h"
#include "core/Parser.h"
#include "core/Storage.h"
#include "core/Paths.h"

using namespace qt_rsync;

class UiTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void compareRendersFakeRsyncOutput();
  void spaceExcludesEverySelectedItem();
  void settingsDialogPersistsDiagnostics();
  void verifyAndSyncKeepComparisonStale();
  void restoresCachedProfileOnStartup();
  void lockedProfileCanQuitWithoutChanges();
  void arrowKeysFoldAndExpandFolders();
  void sshProfileKeepsItsEndpointKind();
  void cancelStopsARunningCompare();
  void deleteDecisionAppearsInSyncRules();
  void mirrorModeTreatsDestinationOnlyAsDeletion();
  void realMirrorSyncDeletesDestinationOnlyFiles();
  void excludedFileIsNotTransferredByRealSync();
  void excludedCheckboxShowsExcludedRows();
  void unchangedOnlyShownWhenToggled();
  void layoutModesRearrangeWidgets();
  void packagingResourcesArePresent();
  void decisionCheckboxStatesAreDistinct();
  void inventoryParserKeepsSizesAndTimes();
  void filterEditorValidatesAndListsRules();
  void actionFiltersHideAndKeepAncestors();
  void filterCheckboxesOnlyTakeKeyboardFocus();
  void actionFilterBarSeparatesEveryPair();
  void sortIndicatorsAndRowsAreLinked();
  void directoryFiltersApplyOnlyWhenEnabled();
  void overviewCountsReportedDirectoriesOnly();
  void createdDirectoriesAreBlankOnDestination();
  void overviewFolderActivationEmitsPath();
  void activatePathRevealsAndFocusesFolder();
  void excludeKeepsFocusOnNearestRow();
  void excludeBatchFocusesAfterLastExcluded();
  void rootHasNoExcludeDecision();
  void overviewSpaceExcludesTheSelectedFolder();
  void homeEndJumpToTheVisibleEdges();
  void ctrlUpDownWalkTheHierarchy();
  void leftRightFoldAndNavigateFolders();
  void filterCheckboxesSupportInvertedClicks();
  void resetExclusionsKeepsOtherDecisions();
  void actionFilterHidesUnrelatedDirectories();
  void staleRootIsMarkedInTheComparison();
  void layoutShortcutCyclesModes();
  void startupOptionsParseAllOptions();
  void startupOptionsValidateModeAndComparePair();
  void menusAndShortcutsAreWired();
  void compareProgressShowsItemsFound();
  void journalEntriesHaveTimestamps();
  void metadataListingStartsBeforeViewBuild();
  void journalCommandLinesAreNeutral();
  void decisionsTooltipIsInformative();
  void useAsLocationDialogChoices();
  void useAsLocationMakesComparisonStale();
  void syncIsDisabledOnStartup();
  void invertedClicksCoverStateCombinations();
  void pathFiltersAndGlobsMatchVisibleRows();
  void startupActionStatesMatchPython();
  void journalSeverityColoursMatchPython();
  void staleRootTooltipAndContextMenuActions();
  void dialogRoundTripsCoverPortedFields();
  void goldenScreenshotsAreValidImages();
  void exitSavePromptOffersSaveAs();
  void headerMenuTogglesExactSections();
  void contextMenuOpenActionsFollowEndpointKind();
  void cellRolesAndPresenceMatchPython();
  void swapEndpointTexts();
  void overviewFollowsActionFilters();
  void contextMenuOffersExternalTools();
  void foldingRefreshesViewsAndOverview();
  void settingsUsePlainTextFormats();
  void startupControlsAndDialogDefaults();
  void commandPreviewIsTemporaryJournalEntry();
  void overviewWidthStableAcrossLayouts();
  void toolbarOrderAndLocationStaleness();
  void escapeClearsSearchAndSearchSelectionReplaces();
};

void UiTest::initTestCase() {
  qt_rsync::app::installTooltipStyle(*qApp);
  QCOMPARE(qApp->style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay), 250);
  QCOMPARE(qApp->style()->styleHint(QStyle::SH_ToolTip_FallAsleepDelay), 0);
}

void UiTest::compareRendersFakeRsyncOutput() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|123|qpy-rsync|2026-11-23|qpy-rsync|docs/a.txt' "
             "'*deleting|qpy-rsync|0|qpy-rsync||qpy-rsync|old.txt' "
             "'.d..t......|qpy-rsync|0|qpy-rsync||qpy-rsync|docs/'\n");
  file.close();
  QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  QVERIFY(edits.size() >= 2);
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));

  QAbstractButton* compare = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
  }
  QVERIFY(compare != nullptr);
  QVERIFY(compare->toolTip().contains(QStringLiteral("rsync dry run")));
  auto* mode = window.findChild<QComboBox*>();
  QVERIFY(mode != nullptr);
  QVERIFY(mode->toolTip().contains(QStringLiteral("Mirror:")));
  compare->click();
  QLineEdit* status = window.findChild<QLineEdit*>("status-label");
  QVERIFY(status != nullptr);
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  QVERIFY(status->text().contains(QStringLiteral("3 changed item(s)")));
  QVERIFY(edits.at(0)->text().endsWith('/'));
  QVERIFY(edits.at(1)->text().endsWith('/'));
  const auto tables = window.findChildren<QTableView*>();
  QCOMPARE(tables.size(), 3);
  QVERIFY(tables.at(0)->model()->rowCount() >= 3);
  auto* progress = window.findChild<QProgressBar*>();
  QVERIFY(progress != nullptr);
  QVERIFY(progress->isHidden());
  mode->setCurrentIndex(1);  // Mirror
  QVERIFY(status->text().contains(QStringLiteral("Stale comparison")));
  qputenv("PATH", oldPath);
}

void UiTest::spaceExcludesEverySelectedItem() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|1|qpy-rsync||qpy-rsync|a.txt' "
             "'>f+++++++++|qpy-rsync|2|qpy-rsync||qpy-rsync|b.txt' "
             "'>f+++++++++|qpy-rsync|3|qpy-rsync||qpy-rsync|c.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);

  auto* comparison = window.findChild<ui::ComparisonView*>();
  QVERIFY(comparison != nullptr);
  const auto tables = window.findChildren<QTableView*>();
  auto* decisionTable = tables.at(1);
  auto* selection = decisionTable->selectionModel();
  const auto flags = QItemSelectionModel::SelectionFlag::Select |
                     QItemSelectionModel::SelectionFlag::Rows;
  QItemSelection itemSelection;
  itemSelection.select(decisionTable->model()->index(1, 4),
                       decisionTable->model()->index(3, 4));
  selection->select(itemSelection, flags);
  QVERIFY(!selection->selectedRows().isEmpty());
  QKeyEvent event(QEvent::KeyPress, Qt::Key_Space, Qt::KeyboardModifier::NoModifier);
  QApplication::sendEvent(decisionTable, &event);

  const auto items = comparison->decisions().explicitItems();
  int excluded = 0;
  for (const auto& item : items) {
    if (item.second == core::Decision::Exclude) ++excluded;
  }
  QVERIFY(excluded >= 3);
  QCheckBox* excludedToggle = nullptr;
  for (QCheckBox* box : window.findChildren<QCheckBox*>()) {
    if (box->accessibleName() == QLatin1String("Show excluded items")) {
      excludedToggle = box;
    }
  }
  QVERIFY(excludedToggle != nullptr);
  QVERIFY(!excludedToggle->text().isEmpty());
  const int hiddenRows = decisionTable->model()->rowCount();

  QAbstractButton* undo = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Undo")) undo = button;
  }
  QVERIFY(undo != nullptr);
  QVERIFY(undo->isEnabled());
  undo->click();
  QVERIFY(comparison->decisions().explicitItems().isEmpty());
  QVERIFY(decisionTable->model()->rowCount() > hiddenRows);
  QVERIFY(!undo->isEnabled());
  qputenv("PATH", oldPath);
}

void UiTest::settingsDialogPersistsDiagnostics() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  ui::SettingsDialog dialog;
  QCOMPARE(dialog.findChildren<QTabWidget*>().size(), 1);
  QCOMPARE(dialog.findChildren<QCheckBox*>().size(), 10);
  bool logEditFound = false;
  for (QLineEdit* edit : dialog.findChildren<QLineEdit*>()) {
    if (edit->text().endsWith(QStringLiteral("app-default.log"))) {
      logEditFound = true;
    }
  }
  QVERIFY(logEditFound);
  bool toolsTableFound = false;
  for (QTableWidget* table : dialog.findChildren<QTableWidget*>()) {
    if (table->columnCount() == 4 &&
        table->horizontalHeaderItem(0) != nullptr &&
        table->horizontalHeaderItem(0)->text() == QLatin1String("Name")) {
      toolsTableFound = true;
    }
  }
  QVERIFY(toolsTableFound);
  for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
    if (box->text().contains(QStringLiteral("timing"))) box->setChecked(true);
    if (box->text().contains(QStringLiteral("Verify Decisions"))) {
      QVERIFY2(!box->isChecked(), "verification must default to off");
    }
  }
  for (QDialogButtonBox* buttons : dialog.findChildren<QDialogButtonBox*>()) {
    for (QAbstractButton* button : buttons->buttons()) {
      if (button->text().contains(QStringLiteral("OK"))) {
        const_cast<QAbstractButton*>(button)->click();
      }
    }
  }
  core::storage::Settings settings;
  QVERIFY(settings.value(QStringLiteral("logging/timing")).toBool());
}

void UiTest::verifyAndSyncKeepComparisonStale() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|1|qpy-rsync||qpy-rsync|a.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  QAbstractButton* compare = nullptr;
  QAbstractButton* verify = nullptr;
  QAbstractButton* sync = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
    if (button->text() == QLatin1String("Verify")) verify = button;
    if (button->text() == QLatin1String("Synchronize")) sync = button;
  }
  QVERIFY(compare && verify && sync);
  compare->click();
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  QVERIFY(verify->isEnabled());
  QVERIFY(sync->isEnabled());

  verify->click();
  QTest::qWait(100);
  QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("Verification")), 5000);

  QTimer::singleShot(100, [&]() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr || !dialog->isVisible() ||
          !dialog->windowTitle().contains(QStringLiteral("synchronization"))) {
        continue;
      }
      for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
        if (box->text().contains(QStringLiteral("Compare again"))) box->setChecked(false);
      }
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text().contains(QStringLiteral("Synchronize"))) {
          button->click();
          break;
        }
      }
      return;
    }
  });
  sync->click();
  QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("click Compare to refresh")), 5000);
  QVERIFY(!status->text().contains(QStringLiteral("Comparison complete")));
  qputenv("PATH", oldPath);
}

void UiTest::restoresCachedProfileOnStartup() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  core::SyncProfile profile;
  profile.name = QStringLiteral("example");
  profile.source.path = QStringLiteral("/src");
  profile.destination.path = QStringLiteral("/dst");
  const QString path = core::paths::profilesDirectory() + QStringLiteral("/example.json");
  core::storage::saveProfile(path, profile);
  core::storage::setCurrentProfilePath(path);

  ui::MainWindow window;
  QVERIFY(!window.startupAborted());
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  QCOMPARE(edits.at(0)->text(), QStringLiteral("/src"));
  QCOMPARE(edits.at(1)->text(), QStringLiteral("/dst"));
  QCOMPARE(core::storage::currentProfilePath(), path);
}

void UiTest::lockedProfileCanQuitWithoutChanges() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  core::SyncProfile profile;
  profile.name = QStringLiteral("locked");
  profile.source.path = QStringLiteral("/src");
  profile.destination.path = QStringLiteral("/dst");
  const QString path = core::paths::profilesDirectory() + QStringLiteral("/locked.json");
  core::storage::saveProfile(path, profile);
  core::storage::setCurrentProfilePath(path);
  QFile lock(core::storage::FileLock(path).lockPath());
  QVERIFY(lock.open(QIODevice::WriteOnly | QIODevice::Truncate));
  lock.write("1");
  lock.close();

  QTimer::singleShot(100, []() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr || !dialog->isVisible()) continue;
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text() == QLatin1String("Quit without changes")) button->click();
      }
      return;
    }
  });
  ui::MainWindow window;
  QVERIFY(window.startupAborted());
  QCOMPARE(core::storage::currentProfilePath(), path);
}

void UiTest::arrowKeysFoldAndExpandFolders() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|6|qpy-rsync||qpy-rsync|docs/a.txt' "
             "'>f+++++++++|qpy-rsync|6|qpy-rsync||qpy-rsync|docs/b.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  const auto tables = window.findChildren<QTableView*>();
  auto* table = tables.at(0);
  const int before = table->model()->rowCount();

  int docsRow = -1;
  for (int row = 0; row < before; ++row) {
    if (table->model()->data(table->model()->index(row, 0), Qt::UserRole).toString() ==
        QLatin1String("docs")) {
      docsRow = row;
      break;
    }
  }
  QVERIFY(docsRow >= 0);
  table->setCurrentIndex(table->model()->index(docsRow, 0));
  QKeyEvent left(QEvent::KeyPress, Qt::Key_Left, Qt::KeyboardModifier::NoModifier);
  QApplication::sendEvent(table, &left);
  QVERIFY(table->model()->rowCount() < before);
  QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::KeyboardModifier::NoModifier);
  QApplication::sendEvent(table, &right);
  QCOMPARE(table->model()->rowCount(), before);
  qputenv("PATH", oldPath);
}

void UiTest::sshProfileKeepsItsEndpointKind() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  core::SyncProfile profile;
  profile.name = QStringLiteral("example");
  profile.source.kind = core::EndpointKind::Ssh;
  profile.source.host = QStringLiteral("example.test");
  profile.source.path = QStringLiteral("/home/user/Develop/src");
  profile.destination.path = QStringLiteral("/home/alice/Develop/src-example/");
  const QString path = core::paths::profilesDirectory() + QStringLiteral("/example.json");
  core::storage::saveProfile(path, profile);
  core::storage::setCurrentProfilePath(path);

  ui::MainWindow window;
  app::StartupOptions emptyOptions;
  window.applyStartupOptions(emptyOptions);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  QCOMPARE(edits.at(0)->text(),
           QStringLiteral("example.test:/home/user/Develop/src"));
  QCOMPARE(edits.at(1)->text(),
           QStringLiteral("/home/alice/Develop/src-example/"));
}

void UiTest::cancelStopsARunningCompare() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\nsleep 10\n"
             "printf '%s\\n' '>f+++++++++|qpy-rsync|1|qpy-rsync||qpy-rsync|a.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  QAbstractButton* compare = nullptr;
  QAbstractButton* cancel = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
    if (button->text() == QLatin1String("Cancel")) cancel = button;
  }
  QVERIFY(compare && cancel);
  compare->click();
  QVERIFY(cancel->isEnabled());
  cancel->click();
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("cancel")), 5000);
  QVERIFY(compare->isEnabled());
  QVERIFY(!cancel->isEnabled());
  qputenv("PATH", oldPath);
}

void UiTest::deleteDecisionAppearsInSyncRules() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' '=f..........|qpy-rsync|5|qpy-rsync||qpy-rsync|.parentlock'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);

  auto* comparison = window.findChild<ui::ComparisonView*>();
  comparison->markDelete(QStringLiteral(".parentlock"));
  QCOMPARE(comparison->decisions().explicitDecision(QStringLiteral(".parentlock")),
           core::Decision::Delete);

  QAbstractButton* sync = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Synchronize")) sync = button;
  }
  QVERIFY(sync != nullptr);
  QTimer::singleShot(100, []() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr ||
          !dialog->windowTitle().contains(QStringLiteral("synchronization"))) {
        continue;
      }
      QString rules;
      for (QPlainTextEdit* text : dialog->findChildren<QPlainTextEdit*>()) {
        if (text->toPlainText().contains(QStringLiteral("-s /"))) {
          rules = text->toPlainText();
        }
      }
      QVERIFY(rules.contains(QStringLiteral("-s /.parentlock")));
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text() == QLatin1String("Cancel")) button->click();
      }
      return;
    }
  });
  sync->click();
  QVERIFY(!status->text().contains(QStringLiteral("Synchronizing")));
  qputenv("PATH", oldPath);
}

void UiTest::mirrorModeTreatsDestinationOnlyAsDeletion() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' '*deleting|qpy-rsync|5|qpy-rsync||qpy-rsync|old.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  options.mode = QStringLiteral("mirror");
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  auto* comparison = window.findChild<ui::ComparisonView*>();
  auto it = comparison->store().path_to_id.find("old.txt");
  QVERIFY(it != comparison->store().path_to_id.end());
  QCOMPARE(comparison->store().actions.at(it->second), 3);  // model::Delete
  qputenv("PATH", oldPath);
}

void UiTest::realMirrorSyncDeletesDestinationOnlyFiles() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString source = dir.path() + QStringLiteral("/src");
  const QString destination = dir.path() + QStringLiteral("/dst");
  QDir().mkpath(source);
  QDir().mkpath(destination);
  QFile file(source + QStringLiteral("/a.txt"));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("new content with a different length\n");
  file.close();
  QFile file2(destination + QStringLiteral("/a.txt"));
  QVERIFY(file2.open(QIODevice::WriteOnly | QIODevice::Text));
  file2.write("old content\n");
  file2.close();
  QFile file3(destination + QStringLiteral("/old.txt"));
  QVERIFY(file3.open(QIODevice::WriteOnly | QIODevice::Text));
  file3.write("stale\n");
  file3.close();

  ui::MainWindow window;
  app::StartupOptions options;
  options.mode = QStringLiteral("mirror");
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(source);
  edits.at(1)->setText(destination);
  QAbstractButton* compare = nullptr;
  QAbstractButton* sync = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
    if (button->text() == QLatin1String("Synchronize")) sync = button;
  }
  QVERIFY(compare && sync);
  compare->click();
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 15000);

  QTimer::singleShot(200, []() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr ||
          !dialog->windowTitle().contains(QStringLiteral("synchronization"))) {
        continue;
      }
      for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
        if (box->text().contains(QStringLiteral("Compare again"))) box->setChecked(false);
      }
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text().contains(QStringLiteral("Synchronize"))) {
          button->click();
        }
      }
      return;
    }
  });
  sync->click();
  qDebug() << "EXCL SYNC STATUS" << status->text();
  QTRY_VERIFY_WITH_TIMEOUT(
      status->text().contains(QStringLiteral("Synchronization complete")), 15000);

  QFile updated(destination + QStringLiteral("/a.txt"));
  QVERIFY(updated.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(updated.readAll().trimmed(),
           QByteArray("new content with a different length"));
  QVERIFY(!QFile::exists(destination + QStringLiteral("/old.txt")));
  QVERIFY(status->text().contains(QStringLiteral("click Compare to refresh")));
}

void UiTest::excludedFileIsNotTransferredByRealSync() {
  QTemporaryDir dir;
  const QString source = dir.path() + QStringLiteral("/src");
  const QString destination = dir.path() + QStringLiteral("/dst");
  QDir().mkpath(source);
  QDir().mkpath(destination);
  QFile a(source + QStringLiteral("/a.txt"));
  QVERIFY(a.open(QIODevice::WriteOnly | QIODevice::Text));
  a.write("new a content that is longer\n");
  a.close();
  QFile b(source + QStringLiteral("/b.txt"));
  QVERIFY(b.open(QIODevice::WriteOnly | QIODevice::Text));
  b.write("new b content that is longer\n");
  b.close();
  QFile oldA(destination + QStringLiteral("/a.txt"));
  QVERIFY(oldA.open(QIODevice::WriteOnly | QIODevice::Text));
  oldA.write("old a content\n");
  oldA.close();
  QFile oldB(destination + QStringLiteral("/b.txt"));
  QVERIFY(oldB.open(QIODevice::WriteOnly | QIODevice::Text));
  oldB.write("old b content\n");
  oldB.close();

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(source);
  edits.at(1)->setText(destination);
  QAbstractButton* compare = nullptr;
  QAbstractButton* sync = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
    if (button->text() == QLatin1String("Synchronize")) sync = button;
  }
  QVERIFY(compare && sync);
  compare->click();
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 15000);

  auto* comparison = window.findChild<ui::ComparisonView*>();
  comparison->applyDecision(QStringLiteral("b.txt"),
                            static_cast<int>(core::Decision::Exclude));
  QTimer::singleShot(200, []() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr ||
          !dialog->windowTitle().contains(QStringLiteral("synchronization"))) continue;
      for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
        if (box->text().contains(QStringLiteral("Compare again"))) box->setChecked(false);
      }
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text().contains(QStringLiteral("Synchronize"))) {
          button->click();
        }
      }
      return;
    }
  });
  sync->click();
  QTRY_VERIFY_WITH_TIMEOUT(
      status->text().contains(QStringLiteral("Synchronization complete")), 15000);

  QFile updatedA(destination + QStringLiteral("/a.txt"));
  QVERIFY(updatedA.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(updatedA.readAll().trimmed(), QByteArray("new a content that is longer"));
  QFile unchanged(destination + QStringLiteral("/b.txt"));
  QVERIFY(unchanged.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(unchanged.readAll().trimmed(), QByteArray("old b content"));
}

void UiTest::excludedCheckboxShowsExcludedRows() {
  QTemporaryDir dir;
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|1|qpy-rsync||qpy-rsync|a.txt' "
             "'>f+++++++++|qpy-rsync|1|qpy-rsync||qpy-rsync|b.txt'\n");
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);

  const auto tables = window.findChildren<QTableView*>();
  const int before = tables.at(0)->model()->rowCount();
  auto* comparison = window.findChild<ui::ComparisonView*>();
  comparison->applyDecision(QStringLiteral("a.txt"),
                            static_cast<int>(core::Decision::Exclude));
  const int hidden = tables.at(0)->model()->rowCount();
  QVERIFY(hidden < before);

  auto* excludedBox = window.findChild<QCheckBox*>("filter-box-1");
  QVERIFY(excludedBox != nullptr);
  excludedBox->setChecked(true);
  QVERIFY(tables.at(0)->model()->rowCount() > hidden);
  qputenv("PATH", oldPath);
}

void UiTest::unchangedOnlyShownWhenToggled() {
  ui::ComparisonView view;
  ui::ActionFilterBar bar;
  view.setFilterBar(&bar);
  QVector<core::Change> changes;
  core::Change changed;
  changed.path = QStringLiteral("new.txt");
  changed.action = core::Action::Create;
  changed.kind = core::Kind::File;
  changed.size = 4;
  core::Change unchanged;
  unchanged.path = QStringLiteral("same.txt");
  unchanged.action = core::Action::NoChange;
  unchanged.kind = core::Kind::File;
  unchanged.size = 4;
  changes << changed << unchanged;
  view.setChanges(changes);
  const auto tables = view.findChildren<QTableView*>();
  QCOMPARE(tables.size(), 3);
  const int changedRows = tables.at(0)->model()->rowCount();
  bar.toggleUnchanged();
  QVERIFY(tables.at(0)->model()->rowCount() > changedRows);
  bar.toggleUnchanged();
  QCOMPARE(tables.at(0)->model()->rowCount(), changedRows);
}

void UiTest::layoutModesRearrangeWidgets() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(20);
  auto* overview = window.findChild<ui::OverviewPane*>();
  auto* comparison = window.findChild<ui::ComparisonView*>();
  QVERIFY(overview != nullptr);
  QVERIFY(comparison != nullptr);
  auto* mainSplitter = qobject_cast<QSplitter*>(comparison->parentWidget());
  QVERIFY(mainSplitter != nullptr);
  const auto action = [&window](const QString& text) -> QAction* {
    for (QAction* candidate : window.findChildren<QAction*>()) {
      if (candidate->text() == text) return candidate;
    }
    return nullptr;
  };
  QAction* focus = action(QStringLiteral("Compare only"));
  QAction* wide = action(QStringLiteral("Compare with overview"));
  QAction* normal = action(QStringLiteral("Normal"));
  QVERIFY(focus && wide && normal);

  focus->trigger();
  QTest::qWait(10);
  QVERIFY(overview->isHidden());
  QCOMPARE(mainSplitter->orientation(), Qt::Vertical);

  wide->trigger();
  QTest::qWait(10);
  QVERIFY(!overview->isHidden());
  QCOMPARE(overview->parentWidget(), static_cast<QWidget*>(mainSplitter));
  QCOMPARE(mainSplitter->orientation(), Qt::Horizontal);

  normal->trigger();
  QTest::qWait(10);
  QVERIFY(!overview->isHidden());
  QCOMPARE(mainSplitter->orientation(), Qt::Vertical);
}

void UiTest::packagingResourcesArePresent() {
  const QString root = QStringLiteral(QT_RSYNC_SOURCE_DIR);
  QFile desktop(root + QStringLiteral("/resources/qt-rsync.desktop"));
  QVERIFY(desktop.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString contents = QString::fromUtf8(desktop.readAll());
  QVERIFY(contents.contains(QStringLiteral("Name=qt-rsync")));
  QVERIFY(contents.contains(QStringLiteral("Exec=qt-rsync")));
  QVERIFY(contents.contains(QStringLiteral("Icon=qt-rsync")));
  QVERIFY(QFile::exists(root + QStringLiteral("/resources/qt-rsync.svg")));
  QVERIFY(!QIcon(QStringLiteral(":/qt-rsync.svg")).isNull());
}

void UiTest::decisionCheckboxStatesAreDistinct() {
  model::DataStore store;
  store.generate(2);
  store.decisions[0] = model::Inherit;
  store.decisions[1] = model::Exclude;
  model::TableModel model(&store, 1);
  ui::NativeDelegate delegate(&store, 1);
  QStyleOptionViewItem option;
  option.rect = QRect(0, 0, 92, 22);
  option.state = QStyle::State_Enabled;
  const auto sample = [&](int row) {
    QPixmap pixmap(92, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    delegate.paint(&painter, option, model.index(row, 4));
    painter.end();
    return pixmap.toImage().pixelColor(28, 5);
  };
  const QColor checked = sample(0);
  const QColor unchecked = sample(1);
  QVERIFY(checked != unchecked);
  QVERIFY(checked.blue() > checked.red());
}

void UiTest::inventoryParserKeepsSizesAndTimes() {
  const QString listing =
      QStringLiteral(
          "-rw-r--r--  1,234 2026/11/24 10:00:00 a.txt\n"
          "drwxr-xr-x  4,096 2026/11/24 10:01:00 docs\n");
  const QVector<core::Change> changes = core::parseInventory(listing);
  QCOMPARE(changes.size(), 2);
  QCOMPARE(changes.at(0).path, QStringLiteral("a.txt"));
  QCOMPARE(changes.at(0).kind, core::Kind::File);
  QCOMPARE(changes.at(0).size.value_or(-1), 1234LL);
  QCOMPARE(changes.at(0).modifiedTime,
           QStringLiteral("2026/11/24-10:00:00"));
  QCOMPARE(changes.at(1).kind, core::Kind::Directory);
  QCOMPARE(changes.at(1).size.value_or(-1), 4096LL);
}

void UiTest::filterEditorValidatesAndListsRules() {
  ui::FilterEditorWidget editor;
  editor.setText(QStringLiteral(
      "# comment\n"
      "*.tmp\n"
      "+ keep.txt\n"
      "P cache/***\n"));
  QCOMPARE(editor.rules().size(), 3);
  QVERIFY(!editor.firstError().has_value());
  editor.setText(QStringLiteral("plain-name\n"));
  QCOMPARE(editor.rules().size(), 1);
  QCOMPARE(editor.rules().first().rule, QStringLiteral("- plain-name"));
  QVERIFY(!editor.firstError().has_value());
}

void UiTest::actionFiltersHideAndKeepAncestors() {
  ui::ComparisonView view;
  ui::ActionFilterBar bar;
  view.setFilterBar(&bar);
  QVector<core::Change> changes;
  core::Change created;
  created.path = QStringLiteral("docs/new.txt");
  created.action = core::Action::Create;
  core::Change updated;
  updated.path = QStringLiteral("old.txt");
  updated.action = core::Action::UpdateContent;
  changes << created << updated;
  view.setChanges(changes);
  const auto tables = view.findChildren<QTableView*>();
  QCOMPARE(tables.size(), 3);
  // filter-box-3 is Create; switch the other four action filters off.
  for (int box = 4; box <= 7; ++box) {
    auto* check = bar.findChild<QCheckBox*>(
        QStringLiteral("filter-box-%1").arg(box));
    if (check != nullptr) check->setChecked(false);
  }
  QSet<QString> names;
  for (int row = 0; row < tables.at(0)->model()->rowCount(); ++row) {
    names.insert(tables.at(0)->model()->index(row, 0).data().toString());
  }
  QVERIFY(names.contains(QStringLiteral("docs")));
  QVERIFY(names.contains(QStringLiteral("new.txt")));
  QVERIFY(!names.contains(QStringLiteral("old.txt")));
}

void UiTest::filterCheckboxesOnlyTakeKeyboardFocus() {
  ui::ActionFilterBar bar;
  const auto boxes = bar.findChildren<QCheckBox*>();
  QVERIFY(!boxes.isEmpty());
  for (QCheckBox* box : boxes) {
    QCOMPARE(box->focusPolicy(), Qt::TabFocus);
  }
}

void UiTest::actionFilterBarSeparatesEveryPair() {
  ui::ActionFilterBar bar;
  int separators = 0;
  for (QFrame* frame : bar.findChildren<QFrame*>()) {
    if (frame->frameShape() == QFrame::VLine) ++separators;
  }
  QCOMPARE(separators, 8);
}

void UiTest::sortIndicatorsAndRowsAreLinked() {
  ui::ComparisonView view;
  ui::ActionFilterBar bar;
  view.setFilterBar(&bar);
  QVector<core::Change> changes;
  const auto add = [&changes](const QString& path, qlonglong size) {
    core::Change change;
    change.path = path;
    change.action = core::Action::Create;
    change.kind = core::Kind::File;
    change.size = size;
    changes.append(change);
  };
  add(QStringLiteral("small.txt"), 10);
  add(QStringLiteral("large.txt"), 1000);
  add(QStringLiteral("medium.txt"), 100);
  view.setChanges(changes);
  const auto tables = view.findChildren<QTableView*>();
  QCOMPARE(tables.size(), 3);
  QHeaderView* sourceHeader = tables.at(0)->horizontalHeader();
  QHeaderView* actionHeader = tables.at(1)->horizontalHeader();
  QHeaderView* destinationHeader = tables.at(2)->horizontalHeader();

  sourceHeader->setSortIndicator(2, Qt::DescendingOrder);
  QTest::qWait(1);
  QCOMPARE(sourceHeader->sortIndicatorSection(), 2);
  QCOMPARE(destinationHeader->sortIndicatorSection(), 7);
  QCOMPARE(actionHeader->sortIndicatorSection(), -1);
  QCOMPARE(sourceHeader->sortIndicatorOrder(), Qt::DescendingOrder);
  QCOMPARE(destinationHeader->sortIndicatorOrder(), Qt::DescendingOrder);

  int firstFileSize = -1;
  for (int id : view.store().visible) {
    if (id < static_cast<int>(view.store().is_dir.size()) &&
        view.store().is_dir[id]) {
      continue;
    }
    firstFileSize = static_cast<int>(view.store().sizes.at(id));
    break;
  }
  QCOMPARE(firstFileSize, 1000);

  destinationHeader->setSortIndicator(6, Qt::AscendingOrder);
  QTest::qWait(1);
  QCOMPARE(sourceHeader->sortIndicatorSection(), 1);
  QCOMPARE(destinationHeader->sortIndicatorSection(), 6);
  QCOMPARE(sourceHeader->sortIndicatorOrder(), Qt::AscendingOrder);
}

void UiTest::directoryFiltersApplyOnlyWhenEnabled() {
  ui::ComparisonView view;
  ui::ActionFilterBar bar;
  view.setFilterBar(&bar);
  QVector<core::Change> changes;
  core::Change metadataDir;
  metadataDir.path = QStringLiteral("empty");
  metadataDir.action = core::Action::UpdateMetadata;
  metadataDir.kind = core::Kind::Directory;
  core::Change docs;
  docs.path = QStringLiteral("docs");
  docs.action = core::Action::UpdateMetadata;
  docs.kind = core::Kind::Directory;
  core::Change child;
  child.path = QStringLiteral("docs/a.txt");
  child.action = core::Action::Create;
  child.kind = core::Kind::File;
  child.size = 1;
  changes << metadataDir << docs << child;
  view.setChanges(changes);
  const auto visible = [&view]() {
    QSet<QString> paths;
    for (int id : view.store().visible) {
      paths.insert(QString::fromStdString(view.store().paths[id]));
    }
    return paths;
  };
  // Turn the Metadata action filter off.
  auto* metadataBox = bar.findChild<QCheckBox*>(QStringLiteral("filter-box-5"));
  QVERIFY(metadataBox != nullptr);
  metadataBox->setChecked(false);
  QTest::qWait(1);
  // A directory with no visible children disappears even when the
  // directory toggle is off. docs stays as the ancestor of its create item.
  QVERIFY(!visible().contains(QStringLiteral("empty")));
  QVERIFY(visible().contains(QStringLiteral("docs")));
  const auto docsId = view.store().path_to_id.find("docs");
  QVERIFY(docsId != view.store().path_to_id.end());
  QVERIFY(view.store().faded.at(docsId->second) == 0);

  bar.setFilterDirectories(true);
  QTest::qWait(1);
  // The same ancestor remains, now faded and uncounted.
  QVERIFY(!visible().contains(QStringLiteral("empty")));
  QVERIFY(visible().contains(QStringLiteral("docs")));
  const auto found = view.store().path_to_id.find("docs");
  QVERIFY(found != view.store().path_to_id.end());
  QVERIFY(view.store().faded.at(found->second) != 0);
}

void UiTest::overviewCountsReportedDirectoriesOnly() {
  ui::ComparisonView view;
  auto makeChange = [](const QString& path, core::Action action, core::Kind kind,
                       qlonglong size) {
    core::Change change;
    change.path = path;
    change.action = action;
    change.kind = kind;
    change.size = size;
    return change;
  };
  QVector<core::Change> changes;
  changes << makeChange(QStringLiteral("a.txt"), core::Action::Create,
                        core::Kind::File, 100)
          << makeChange(QStringLiteral("b.txt"), core::Action::Create,
                        core::Kind::File, 50)
          << makeChange(QStringLiteral("tools"), core::Action::UpdateMetadata,
                        core::Kind::Directory, 0)
          << makeChange(QStringLiteral("tools/c.py"), core::Action::Create,
                        core::Kind::File, 25)
          << makeChange(QStringLiteral("plain/d.txt"), core::Action::Create,
                        core::Kind::File, 5);
  view.setChanges(changes);
  const auto& store = view.store();
  const auto rootIt = store.path_to_id.find(model::DataStore::root_path());
  QVERIFY(rootIt != store.path_to_id.end());
  const int root = rootIt->second;
  // a.txt, b.txt, the reported tools folder, tools/c.py and plain/d.txt.
  // The synthesized plain folder itself is not an item.
  QCOMPARE(store.dir_items.at(root), 5LL);

  model::OverviewModel overview;
  overview.build(store, true);
  const QModelIndex total = overview.index(0, 0);
  QVERIFY(total.isValid());
  QCOMPARE(overview.data(total.siblingAtColumn(2)).toLongLong(),
           store.dir_items.at(root));

  auto findChild = [&overview](const QModelIndex& parent, const QString& name) {
    for (int row = 0; row < overview.rowCount(parent); ++row) {
      const QModelIndex index = overview.index(row, 0, parent);
      if (overview.data(index).toString() == name) return index;
    }
    return QModelIndex();
  };
  const QModelIndex files = findChild(total, QStringLiteral("Files"));
  QVERIFY(files.isValid());
  // Root objects: a.txt, b.txt and the reported tools directory.
  QCOMPARE(overview.data(files.siblingAtColumn(2)).toLongLong(), 3LL);
  const QModelIndex tools = findChild(total, QStringLiteral("tools"));
  QVERIFY(tools.isValid());
  QCOMPARE(overview.data(tools.siblingAtColumn(2)).toLongLong(), 1LL);
  const QModelIndex plain = findChild(total, QStringLiteral("plain"));
  QVERIFY(plain.isValid());
  QCOMPARE(overview.data(plain.siblingAtColumn(2)).toLongLong(), 1LL);
}

void UiTest::createdDirectoriesAreBlankOnDestination() {
  ui::ComparisonView view;
  core::Change folder;
  folder.path = QStringLiteral("newdir");
  folder.action = core::Action::Create;
  folder.kind = core::Kind::Directory;
  core::Change child;
  child.path = QStringLiteral("newdir/a.txt");
  child.action = core::Action::Create;
  child.kind = core::Kind::File;
  child.size = 1;
  view.setChanges({folder, child});
  auto& store = view.store();
  const auto found = store.path_to_id.find("newdir");
  QVERIFY(found != store.path_to_id.end());
  const int id = found->second;
  QVERIFY(store.present_on_pane(id, 0));
  QVERIFY(!store.present_on_pane(id, 2));
  QVERIFY(store.row_of_id.at(id) >= 0);
  const int row = store.row_of_id.at(id);
  model::TableModel destination(&store, 2);
  QCOMPARE(destination.data(destination.index(row, 5), Qt::DisplayRole).toString(),
           QString());
  QVERIFY(!destination.data(destination.index(row, 5), Qt::DecorationRole).isValid());

  // Painting must not draw the collapse chevron or the folder icon on the
  // side where the directory does not exist.
  ui::NativeDelegate delegate(&store, 2);
  QStyleOptionViewItem option;
  option.rect = QRect(0, 0, 240, 22);
  option.state = QStyle::State_Enabled;
  option.palette = QApplication::palette();
  QPixmap pixmap(240, 22);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  delegate.paint(&painter, option, destination.index(row, 5));
  painter.end();
  const QImage image = pixmap.toImage();
  const QColor background = image.pixelColor(200, 11);
  for (int x = 4; x < 80; ++x) {
    QVERIFY2(image.pixelColor(x, 11) == background,
             qPrintable(QStringLiteral("icon pixel at %1").arg(x)));
  }
}

void UiTest::overviewFolderActivationEmitsPath() {
  ui::ComparisonView view;
  core::Change first;
  first.path = QStringLiteral("docs/a.txt");
  first.action = core::Action::Create;
  first.kind = core::Kind::File;
  first.size = 1;
  core::Change second;
  second.path = QStringLiteral("docs/sub/b.txt");
  second.action = core::Action::Create;
  second.kind = core::Kind::File;
  second.size = 1;
  view.setChanges({first, second});

  ui::OverviewPane overview;
  overview.rebuild(view.store());
  auto* tree = overview.findChild<QTreeView*>();
  QVERIFY(tree != nullptr);
  QAbstractItemModel* model = tree->model();
  QVERIFY(model != nullptr);
  const QModelIndex total = model->index(0, 0);
  QVERIFY(total.isValid());
  QModelIndex docs;
  for (int row = 0; row < model->rowCount(total); ++row) {
    const QModelIndex index = model->index(row, 0, total);
    if (model->data(index).toString() == QStringLiteral("docs")) {
      docs = index;
      break;
    }
  }
  QVERIFY(docs.isValid());
  overview.resize(520, 260);
  overview.show();
  QTest::qWait(1);
  QSignalSpy spy(&overview, &ui::OverviewPane::folderActivated);
  const QRect row = tree->visualRect(docs);
  QVERIFY(row.isValid());
  // Only the jump icon in the branch gutter reveals the folder.
  QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(9, row.center().y()));
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("docs"));
  // Clicking the folder name keeps the default select/expand behaviour.
  QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(row.center().x(), row.center().y()));
  QCOMPARE(spy.count(), 0);
  overview.close();
}

void UiTest::activatePathRevealsAndFocusesFolder() {
  ui::ComparisonView view;
  core::Change first;
  first.path = QStringLiteral("docs/a.txt");
  first.action = core::Action::Create;
  first.kind = core::Kind::File;
  first.size = 1;
  core::Change second;
  second.path = QStringLiteral("docs/sub/b.txt");
  second.action = core::Action::Create;
  second.kind = core::Kind::File;
  second.size = 1;
  view.setChanges({first, second});
  auto& store = view.store();
  const auto docs = store.path_to_id.find("docs");
  QVERIFY(docs != store.path_to_id.end());
  store.set_collapsed(docs->second, true);
  store.rebuild_visible();
  view.show();
  QTest::qWait(1);
  view.activatePath(QStringLiteral("docs/sub"));
  const auto panes = view.findChildren<QTableView*>();
  QCOMPARE(panes.size(), 3);
  for (QTableView* pane : panes) {
    const QModelIndex current = pane->currentIndex();
    QVERIFY(current.isValid());
    const int row = current.row();
    QVERIFY(row >= 0 && row < static_cast<int>(store.visible.size()));
    QCOMPARE(QString::fromStdString(store.paths[store.visible[row]]),
             QStringLiteral("docs/sub"));
  }
  QVERIFY(!store.is_collapsed(docs->second));
  QTRY_VERIFY(panes.first()->hasFocus());
  view.close();
}

namespace {
QString currentComparisonPath(ui::ComparisonView& view) {
  const auto panes = view.findChildren<QTableView*>();
  if (panes.isEmpty()) return {};
  const QModelIndex current = panes.first()->currentIndex();
  if (!current.isValid()) return {};
  const int row = current.row();
  auto& store = view.store();
  if (row < 0 || row >= static_cast<int>(store.visible.size())) return {};
  return QString::fromStdString(store.paths[store.visible[row]]);
}

QVector<core::Change> fileChanges(const QStringList& paths) {
  QVector<core::Change> changes;
  for (const QString& path : paths) {
    core::Change change;
    change.path = path;
    change.action = core::Action::Create;
    change.kind = core::Kind::File;
    change.size = 1;
    changes << change;
  }
  return changes;
}
}  // namespace

void UiTest::excludeKeepsFocusOnNearestRow() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt"),
                               QStringLiteral("c.txt")}));
  const QString root = QString::fromStdString(model::DataStore::root_path());

  view.selectPath(QStringLiteral("b.txt"));
  view.applyDecision(QStringLiteral("b.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QCOMPARE(currentComparisonPath(view), QStringLiteral("c.txt"));

  view.selectPath(QStringLiteral("c.txt"));
  view.applyDecision(QStringLiteral("c.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QCOMPARE(currentComparisonPath(view), QStringLiteral("a.txt"));

  view.selectPath(QStringLiteral("a.txt"));
  view.applyDecision(QStringLiteral("a.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QCOMPARE(currentComparisonPath(view), root);
}

void UiTest::excludeBatchFocusesAfterLastExcluded() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt"),
                               QStringLiteral("c.txt"), QStringLiteral("d.txt")}));
  const QString root = QString::fromStdString(model::DataStore::root_path());
  // Keep the cursor on an item before the batch: the focus still follows the
  // last excluded item, not the remembered row number.
  view.selectPath(QStringLiteral("a.txt"));
  QVERIFY(QMetaObject::invokeMethod(&view, "startDecisionBatch"));
  view.applyDecision(QStringLiteral("b.txt"),
                     static_cast<int>(core::Decision::Exclude));
  view.applyDecision(QStringLiteral("c.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QVERIFY(QMetaObject::invokeMethod(&view, "endDecisionBatch"));
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d.txt"));

  // Nothing after the batch: use the item before the first excluded row.
  QVERIFY(QMetaObject::invokeMethod(&view, "startDecisionBatch"));
  view.applyDecision(QStringLiteral("c.txt"),
                     static_cast<int>(core::Decision::Exclude));
  view.applyDecision(QStringLiteral("d.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QVERIFY(QMetaObject::invokeMethod(&view, "endDecisionBatch"));
  QCOMPARE(currentComparisonPath(view), QStringLiteral("a.txt"));

  // Everything gone: the root row remains selected.
  QVERIFY(QMetaObject::invokeMethod(&view, "startDecisionBatch"));
  view.applyDecision(QStringLiteral("a.txt"),
                     static_cast<int>(core::Decision::Exclude));
  QVERIFY(QMetaObject::invokeMethod(&view, "endDecisionBatch"));
  QCOMPARE(currentComparisonPath(view), root);
}

void UiTest::rootHasNoExcludeDecision() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
  auto& store = view.store();
  const QString root = QString::fromStdString(model::DataStore::root_path());
  const auto rootId = store.path_to_id.find(root.toStdString());
  QVERIFY(rootId != store.path_to_id.end());
  const int row = store.row_of_id.at(rootId->second);
  QVERIFY(row >= 0);
  model::TableModel decisions(&store, 1);
  const QModelIndex index = decisions.index(row, 4);
  QVERIFY(!decisions.data(index, Qt::CheckStateRole).isValid());
  QVERIFY(!(decisions.flags(index) & Qt::ItemIsUserCheckable));
  QVERIFY(!decisions.setData(index, Qt::Checked, Qt::CheckStateRole));

  view.selectPath(root);
  auto* filter = view.findChild<ui::DecisionKeyFilter*>();
  QVERIFY(filter != nullptr);
  int toggles = 0;
  connect(filter, &ui::DecisionKeyFilter::decisionToggled,
          [&toggles](const QString&, int) { ++toggles; });
  auto* source = view.findChildren<QTableView*>().first();
  QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
  QApplication::sendEvent(source->viewport(), &space);
  QCOMPARE(toggles, 0);
  QVERIFY(store.row_of_id.at(rootId->second) >= 0);
  QVERIFY(view.decisions().explicitItems().isEmpty());

  const QString beforeFocus = root;
  QCOMPARE(currentComparisonPath(view), beforeFocus);
  view.applyDecision(root, static_cast<int>(core::Decision::Exclude));
  QCOMPARE(view.decisions().explicitItems().size(), 0);
  QCOMPARE(store.visible.size(), static_cast<std::size_t>(3));
}

void UiTest::overviewSpaceExcludesTheSelectedFolder() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("docs/a.txt"),
                               QStringLiteral("docs/b.txt"),
                               QStringLiteral("other/c.txt")}));
  ui::OverviewPane overview;
  overview.rebuild(view.store());
  overview.resize(520, 260);
  overview.show();
  QTest::qWait(1);
  auto* tree = overview.findChild<QTreeView*>();
  QVERIFY(tree != nullptr);
  QAbstractItemModel* model = tree->model();
  QVERIFY(model != nullptr);
  const QModelIndex total = model->index(0, 0);
  QVERIFY(total.isValid());
  QModelIndex docs;
  for (int row = 0; row < model->rowCount(total); ++row) {
    const QModelIndex index = model->index(row, 0, total);
    if (model->data(index).toString() == QStringLiteral("docs")) {
      docs = index;
      break;
    }
  }
  QVERIFY(docs.isValid());
  QSignalSpy spy(&overview, &ui::OverviewPane::excludeRequested);
  tree->setCurrentIndex(docs);
  QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
  QApplication::sendEvent(tree, &space);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("docs"));

  // Total is the container and is not an action target.
  tree->setCurrentIndex(total);
  QApplication::sendEvent(tree, &space);
  QCOMPARE(spy.count(), 0);
  overview.close();
}

void UiTest::homeEndJumpToTheVisibleEdges() {
  ui::ComparisonView view;
  QVector<core::Change> changes;
  for (int index = 0; index < 5; ++index) {
    core::Change change;
    change.path = QStringLiteral("d/f%1.txt").arg(index);
    change.action = core::Action::Create;
    change.kind = core::Kind::File;
    change.size = 1;
    changes << change;
  }
  view.setChanges(changes);
  view.show();
  QTest::qWait(1);
  auto* source = view.findChildren<QTableView*>().first();
  view.selectPath(QStringLiteral("d/f2.txt"));
  QTest::keyClick(source, Qt::Key_Home);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d/f0.txt"));
  QTest::keyClick(source, Qt::Key_End);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d/f4.txt"));
  view.close();
}

void UiTest::ctrlUpDownWalkTheHierarchy() {
  ui::ComparisonView view;
  QVector<core::Change> changes;
  for (const QString& path : {QStringLiteral("d1/a.txt"),
                              QStringLiteral("d1/sub/b.txt"),
                              QStringLiteral("d2/c.txt")}) {
    core::Change change;
    change.path = path;
    change.action = core::Action::Create;
    change.kind = core::Kind::File;
    change.size = 1;
    changes << change;
  }
  view.setChanges(changes);
  view.show();
  QTest::qWait(1);
  auto* source = view.findChildren<QTableView*>().first();
  view.selectPath(QStringLiteral("d1/sub/b.txt"));
  QTest::keyClick(source, Qt::Key_Up, Qt::ControlModifier);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d1/sub"));
  QTest::keyClick(source, Qt::Key_Up, Qt::ControlModifier);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d1"));
  QTest::keyClick(source, Qt::Key_Down, Qt::ControlModifier);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("d1/a.txt"));
  view.close();
}

void UiTest::leftRightFoldAndNavigateFolders() {
  ui::ComparisonView view;
  core::Change folder;
  folder.path = QStringLiteral("docs");
  folder.action = core::Action::UpdateMetadata;
  folder.kind = core::Kind::Directory;
  core::Change first;
  first.path = QStringLiteral("docs/a.txt");
  first.action = core::Action::Create;
  first.kind = core::Kind::File;
  first.size = 1;
  core::Change second;
  second.path = QStringLiteral("docs/b.txt");
  second.action = core::Action::Create;
  second.kind = core::Kind::File;
  second.size = 1;
  view.setChanges({folder, first, second});
  view.show();
  QTest::qWait(1);
  auto* source = view.findChildren<QTableView*>().first();
  const auto docsId = view.store().path_to_id.find("docs");
  QVERIFY(docsId != view.store().path_to_id.end());

  view.selectPath(QStringLiteral("docs/a.txt"));
  QTest::keyClick(source, Qt::Key_Left);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("docs"));

  QTest::keyClick(source, Qt::Key_Right);
  QCOMPARE(currentComparisonPath(view), QStringLiteral("docs/a.txt"));

  view.selectPath(QStringLiteral("docs"));
  QTest::keyClick(source, Qt::Key_Left);
  QVERIFY(view.store().is_collapsed(docsId->second));
  QTest::keyClick(source, Qt::Key_Left);
  QCOMPARE(currentComparisonPath(view),
           QString::fromStdString(model::DataStore::root_path()));

  QTest::keyClick(source, Qt::Key_Right);
  QTest::keyClick(source, Qt::Key_Right);
  QVERIFY(view.store().row_of_id.at(docsId->second) >= 0);
  view.close();
}

void UiTest::filterCheckboxesSupportInvertedClicks() {
  ui::ActionFilterBar bar;
  auto* create = bar.findChild<QCheckBox*>(QStringLiteral("filter-box-3"));
  QVERIFY(create != nullptr);
  QCOMPARE(create->contextMenuPolicy(), Qt::CustomContextMenu);
  auto* excluded = bar.findChild<QCheckBox*>(QStringLiteral("filter-box-1"));
  QVERIFY(excluded != nullptr);
  QCOMPARE(excluded->contextMenuPolicy(), Qt::CustomContextMenu);
  QVERIFY(bar.actionVisible(0));
  const auto ctrlClick = [](QCheckBox* box) {
    QPointF point(5, 5);
    QMouseEvent press(QEvent::MouseButtonPress, point, point,
                      Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, point,
                        Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(box, &press);
    QApplication::sendEvent(box, &release);
  };
  ctrlClick(create);
  QVERIFY(bar.actionVisible(0));
  for (int action = 1; action < 5; ++action) {
    QVERIFY(!bar.actionVisible(action));
  }
  ctrlClick(create);
  for (int action = 0; action < 5; ++action) {
    QVERIFY(bar.actionVisible(action));
  }
  ctrlClick(excluded);
  QVERIFY(bar.showExcluded());
  for (int action = 0; action < 5; ++action) {
    QVERIFY(!bar.actionVisible(action));
  }
  ctrlClick(excluded);
  QVERIFY(!bar.showExcluded());
  for (int action = 0; action < 5; ++action) {
    QVERIFY(bar.actionVisible(action));
  }

  // A second cycle with an actual exclusion must show only excluded items.
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt"),
                               QStringLiteral("c.txt")}));
  view.setFilterBar(&bar);
  view.applyDecision(QStringLiteral("b.txt"),
                     static_cast<int>(core::Decision::Exclude));
  ctrlClick(excluded);
  QVERIFY(bar.showExcluded());
  QStringList visiblePaths;
  for (int id : view.store().visible) {
    if (id >= 0 && id < static_cast<int>(view.store().paths.size())) {
      visiblePaths.append(QString::fromStdString(view.store().paths[id]));
    }
  }
  QVERIFY(visiblePaths.contains(QStringLiteral("b.txt")));
  QVERIFY(!visiblePaths.contains(QStringLiteral("a.txt")));
  QVERIFY(!visiblePaths.contains(QStringLiteral("c.txt")));
}

void UiTest::resetExclusionsKeepsOtherDecisions() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt"),
                               QStringLiteral("c.txt")}));
  view.applyDecision(QStringLiteral("a.txt"),
                     static_cast<int>(core::Decision::Exclude));
  view.applyDecision(QStringLiteral("b.txt"),
                     static_cast<int>(core::Decision::Delete));
  QVERIFY(view.decisions().explicitDecision(QStringLiteral("a.txt")) ==
          core::Decision::Exclude);
  view.resetExclusions();
  QCOMPARE(view.decisions().explicitDecision(QStringLiteral("a.txt")),
           core::Decision::Inherit);
  QCOMPARE(view.decisions().explicitDecision(QStringLiteral("b.txt")),
           core::Decision::Delete);
  QCOMPARE(view.store().row_of_id.at(
               view.store().path_to_id.at("a.txt")),
           1);
}

void UiTest::actionFilterHidesUnrelatedDirectories() {
  ui::ComparisonView view;
  auto makeChange = [](const QString& path, core::Action action,
                       core::Kind kind) {
    core::Change change;
    change.path = path;
    change.action = action;
    change.kind = kind;
    change.size = 1;
    return change;
  };
  view.setChanges({
      makeChange(QStringLiteral("meta"), core::Action::UpdateMetadata,
                 core::Kind::Directory),
      makeChange(QStringLiteral("meta/new.txt"), core::Action::Create,
                 core::Kind::File),
      makeChange(QStringLiteral("unrelated"), core::Action::UpdateContent,
                 core::Kind::Directory),
      makeChange(QStringLiteral("unrelated/old.txt"), core::Action::UpdateContent,
                 core::Kind::File),
      makeChange(QStringLiteral("newdir"), core::Action::Create,
                 core::Kind::Directory),
      makeChange(QStringLiteral("newdir/file.txt"), core::Action::Create,
                 core::Kind::File),
  });
  auto& store = view.store();
  store.action_mask = 1 << static_cast<int>(model::Create);
  store.rebuild_visible();

  QSet<QString> visible;
  for (int id : store.visible) {
    visible.insert(QString::fromStdString(store.paths[id]));
  }
  QVERIFY(visible.contains(QStringLiteral("meta")));
  QVERIFY(visible.contains(QStringLiteral("meta/new.txt")));
  QVERIFY(visible.contains(QStringLiteral("newdir")));
  QVERIFY(visible.contains(QStringLiteral("newdir/file.txt")));
  QVERIFY(!visible.contains(QStringLiteral("unrelated")));
  QVERIFY(!visible.contains(QStringLiteral("unrelated/old.txt")));

  const auto root = store.path_to_id.find(model::DataStore::root_path());
  QVERIFY(root != store.path_to_id.end());
  // meta + meta/new.txt + newdir + newdir/file.txt; the unrelated subtree is
  // not an ancestor of anything visible and must not be counted.
  QCOMPARE(store.dir_items.at(root->second), 4LL);
  QCOMPARE(store.dir_items.at(store.path_to_id.at("meta")), 1LL);
  QCOMPARE(store.dir_items.at(store.path_to_id.at("newdir")), 1LL);
}

void UiTest::staleRootIsMarkedInTheComparison() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt")}));
  auto& store = view.store();
  const auto root = store.path_to_id.find(model::DataStore::root_path());
  QVERIFY(root != store.path_to_id.end());
  const int row = store.row_of_id.at(root->second);
  QVERIFY(row >= 0);
  const auto tables = view.findChildren<QTableView*>();
  const QModelIndex index = tables.first()->model()->index(row, 0);
  ui::NativeDelegate delegate(&store, 0);
  QStyleOptionViewItem option;
  option.rect = QRect(0, 0, 320, 22);
  option.state = QStyle::State_Enabled;
  option.palette = QApplication::palette();
  const auto paint = [&]() {
    QPixmap pixmap(320, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    delegate.paint(&painter, option, index);
    painter.end();
    return pixmap.toImage();
  };
  const QImage fresh = paint();
  view.setStale(true);
  QVERIFY(store.stale);
  const QImage stale = paint();
  QVERIFY(stale != fresh);
  view.setStale(false);
  QVERIFY(!store.stale);
}

void UiTest::layoutShortcutCyclesModes() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(20);
  QShortcut* next = nullptr;
  QShortcut* previous = nullptr;
  for (QShortcut* shortcut : window.findChildren<QShortcut*>()) {
    if (shortcut->key() == QKeySequence(QStringLiteral("Ctrl+Tab"))) {
      next = shortcut;
    }
    if (shortcut->key() == QKeySequence(QStringLiteral("Ctrl+Shift+Tab"))) {
      previous = shortcut;
    }
  }
  QVERIFY(next != nullptr);
  QVERIFY(previous != nullptr);
  auto* overview = window.findChild<ui::OverviewPane*>();
  QVERIFY(overview != nullptr);
  const bool initial = overview->isVisible();
  next->activated();
  QTest::qWait(10);
  QVERIFY(overview->isVisible() != initial);
  previous->activated();
  QTest::qWait(10);
  QCOMPARE(overview->isVisible(), initial);
}

void UiTest::startupOptionsParseAllOptions() {
  QString error;
  const auto options = app::StartupOptions::parse(
      {QStringLiteral("qt-rsync"), QStringLiteral("--dir-pair"),
       QStringLiteral("/src/"), QStringLiteral("/dst/"),
       QStringLiteral("--backup"), QStringLiteral("/backup/"),
       QStringLiteral("--mode"), QStringLiteral("new-only"),
       QStringLiteral("--compare"), QStringLiteral("--log-file"),
       QStringLiteral("/tmp/run.log"), QStringLiteral("-style"),
       QStringLiteral("Fusion")}, &error);
  QVERIFY(error.isEmpty());
  QCOMPARE(options.source, QStringLiteral("/src/"));
  QCOMPARE(options.destination, QStringLiteral("/dst/"));
  QCOMPARE(options.backup, QStringLiteral("/backup/"));
  QCOMPARE(options.mode, QStringLiteral("new-only"));
  QVERIFY(options.compare);
  QCOMPARE(options.logFile, QStringLiteral("/tmp/run.log"));
  QVERIFY(!options.noLogFile);
  const QStringList expectedQt{QStringLiteral("-style"), QStringLiteral("Fusion")};
  QCOMPARE(options.qtArguments, expectedQt);

  const auto disabled = app::StartupOptions::parse(
      {QStringLiteral("qt-rsync"), QStringLiteral("--no-log-file")}, &error);
  QVERIFY(error.isEmpty());
  QVERIFY(disabled.noLogFile);
}

void UiTest::startupOptionsValidateModeAndComparePair() {
  QString error;
  app::StartupOptions::parse(
      {QStringLiteral("qt-rsync"), QStringLiteral("--compare")}, &error);
  QVERIFY(error.contains(QStringLiteral("dir-pair")));
  error.clear();
  const auto invalid = app::StartupOptions::parse(
      {QStringLiteral("qt-rsync"), QStringLiteral("--dir-pair"),
       QStringLiteral("/src/"), QStringLiteral("/dst/"),
       QStringLiteral("--mode"), QStringLiteral("two-way")}, &error);
  QVERIFY(error.contains(QStringLiteral("Invalid mode")));
  QCOMPARE(invalid.mode, QStringLiteral("update"));
}

void UiTest::menusAndShortcutsAreWired() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  const auto action = [&window](const QString& text) -> QAction* {
    for (QAction* candidate : window.findChildren<QAction*>()) {
      if (candidate->text() == text) return candidate;
    }
    return nullptr;
  };
  QVERIFY(action(QStringLiteral("Compare only")) != nullptr);
  QVERIFY(action(QStringLiteral("Compare with overview")) != nullptr);
  QVERIFY(action(QStringLiteral("Normal")) != nullptr);
  QVERIFY(action(QStringLiteral("Undo Decision")) != nullptr);
  QVERIFY(action(QStringLiteral("Redo Decision")) != nullptr);

  QStringList menuTitles;
  for (QMenu* menu : window.menuBar()->findChildren<QMenu*>()) {
    menuTitles << menu->title();
  }
  QVERIFY(menuTitles.contains(QStringLiteral("File")));
  QVERIFY(menuTitles.contains(QStringLiteral("Edit")));
  QVERIFY(menuTitles.contains(QStringLiteral("View")));
  QVERIFY(menuTitles.contains(QStringLiteral("Decisions")));
  QVERIFY(menuTitles.contains(QStringLiteral("Help")));

  QSet<QString> shortcutKeys;
  for (QShortcut* shortcut : window.findChildren<QShortcut*>()) {
    shortcutKeys.insert(shortcut->key().toString());
  }
  for (Qt::Key key : {Qt::Key_F1, Qt::Key_F2, Qt::Key_F3, Qt::Key_F4,
                      Qt::Key_F5, Qt::Key_F6, Qt::Key_F7, Qt::Key_F8,
                      Qt::Key_F9, Qt::Key_F10, Qt::Key_F11, Qt::Key_F12}) {
    QVERIFY2(shortcutKeys.contains(QKeySequence(key).toString()),
             qPrintable(QStringLiteral("missing F-key shortcut %1").arg(
                 static_cast<int>(key))));
  }

  QToolButton* sync = nullptr;
  for (QToolButton* tool : window.findChildren<QToolButton*>()) {
    if (tool->accessibleName() == QStringLiteral("Synchronize")) sync = tool;
  }
  QVERIFY(sync != nullptr);
  QToolButton* preserve = nullptr;
  for (QToolButton* tool : window.findChildren<QToolButton*>()) {
    if (tool->text() == QStringLiteral("Preserve")) preserve = tool;
  }
  QVERIFY(preserve != nullptr);
  QVERIFY(preserve->isEnabled());
}

void UiTest::compareProgressShowsItemsFound() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "case \"$*\" in *--list-only*) exit 0 ;; esac\n"
             "i=0\n"
             "while [ $i -lt 1000 ]; do\n"
             "  printf '%s\\n' '>f+++++++++|qpy-rsync|1|qpy-rsync|2026-11-23|qpy-rsync|docs/file$i.txt'\n"
             "  i=$((i+1))\n"
             "done\n"
             "sleep 2\n");
  file.close();
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QVERIFY(status != nullptr);
  const auto progressText = [status]() { return status->text(); };
  auto* progress = window.findChild<QProgressBar*>();
  QVERIFY(progress != nullptr);
  QTRY_VERIFY_WITH_TIMEOUT(!progressText().isEmpty(), 3000);
  QTRY_VERIFY_WITH_TIMEOUT(progressText().contains(QStringLiteral("1000 item(s) found")), 5000);
  QVERIFY(progress->isVisible() || progress->maximum() == 0);
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 8000);
  qputenv("PATH", oldPath);
}

void UiTest::journalEntriesHaveTimestamps() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "case \"$*\" in *--list-only*) exit 0 ;; esac\n"
             "printf '%s\\n' '>f+++++++++|qpy-rsync|1|qpy-rsync|2026-11-23|qpy-rsync|docs/a.txt'\n");
  file.close();
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 8000);
  QPlainTextEdit* journal = nullptr;
  for (QPlainTextEdit* edit : window.findChildren<QPlainTextEdit*>()) {
    if (edit->objectName() != QStringLiteral("command-preview-label")) {
      journal = edit;
    }
  }
  QVERIFY(journal != nullptr);
  const QRegularExpression stamped(QStringLiteral("^\\d{2}:\\d{2}:\\d{2}\\.\\d{1,3} "));
  const QStringList lines = journal->toPlainText().split('\n');
  int checked = 0;
  for (const QString& line : lines) {
    if (line.trimmed().isEmpty()) continue;
    // The command preview is a temporary journal entry: no timestamp and no
    // log-file persistence.
    if (line.contains(QStringLiteral("rsync preview: $ "))) continue;
    QVERIFY2(stamped.match(line).hasMatch(), qPrintable(line));
    ++checked;
  }
  QVERIFY(checked > 4);
  qputenv("PATH", oldPath);
}

void UiTest::metadataListingStartsBeforeViewBuild() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "case \"$*\" in\n"
             "  *--dry-run*)\n"
             "    printf '%s\\n' "
             "'.d..tp.....|qpy-rsync|4096|qpy-rsync|2026-11-23|qpy-rsync|docs/'\n"
             "    ;;\n"
             "  *--list-only*) sleep 1; printf '%s\\n' "
             "'drwxr-xr-x          4,096 2026/11/25 11:23:22 ./' ;;\n"
             "esac\n");
  file.close();
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 8000);

  QPlainTextEdit* journal = nullptr;
  for (QPlainTextEdit* edit : window.findChildren<QPlainTextEdit*>()) {
    if (edit->objectName() != QStringLiteral("command-preview-label")) journal = edit;
  }
  QVERIFY(journal != nullptr);
  const QString text = journal->toPlainText();
  const int listing = text.indexOf(QStringLiteral("$ rsync -r --list-only"));
  const int parse = text.indexOf(QStringLiteral("Compare parse:"));
  QVERIFY2(listing >= 0, qPrintable(text));
  QVERIFY2(parse >= 0, qPrintable(text));
  QVERIFY2(listing < parse, qPrintable(text));
  qputenv("PATH", oldPath);
}

void UiTest::journalCommandLinesAreNeutral() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "case \"$*\" in\n"
             "  *--dry-run*) printf '%s\\n' "
             "'.d..tp.....|qpy-rsync|4096|qpy-rsync|2026-11-23|qpy-rsync|docs/' ;;\n"
             "  *--list-only*) printf '%s\\n' "
             "'drwxr-xr-x          4,096 2026/11/25 11:23:22 ./' ;;\n"
             "esac\n");
  file.close();
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 8000);

  QPlainTextEdit* journal = nullptr;
  for (QPlainTextEdit* edit : window.findChildren<QPlainTextEdit*>()) {
    if (edit->objectName() != QStringLiteral("command-preview-label")) journal = edit;
  }
  QVERIFY(journal != nullptr);
  QTextCursor cursor = journal->document()->find(QStringLiteral("$ rsync"));
  QVERIFY(!cursor.isNull());
  QCOMPARE(cursor.charFormat().foreground().color(),
           journal->palette().color(QPalette::Text));
  qputenv("PATH", oldPath);
}

void UiTest::decisionsTooltipIsInformative() {
  QVector<core::Change> changes;
  core::Change change;
  change.path = QStringLiteral("docs/a.txt");
  change.action = core::Action::Create;
  change.kind = core::Kind::File;
  change.size = 1024;
  change.modifiedTime = QStringLiteral("2026-11-23-11:23:22");
  changes.append(change);

  ui::ComparisonView view;
  view.setChanges(changes);
  auto& store = view.store();
  const auto found = store.path_to_id.find("docs/a.txt");
  QVERIFY(found != store.path_to_id.end());
  const int id = found->second;
  store.dest_times.resize(store.paths.size());
  store.dest_times[id] = "2026-11-24-10:10:10";

  model::TableModel decisions(&store, 1);
  const QModelIndex index = decisions.index(store.row_of_id.at(id), 4);
  const QString tip = decisions.data(index, Qt::ToolTipRole).toString();
  QVERIFY(tip.contains(QStringLiteral("Source:")));
  QVERIFY(tip.contains(QStringLiteral("Destination:")));
  QVERIFY(tip.contains(QStringLiteral("new")));
  QVERIFY(tip.contains(QStringLiteral("Modified:")));
  QVERIFY(tip.contains(QStringLiteral("source 2026-11-23-11:23:22")));
  QVERIFY(tip.contains(QStringLiteral("destination 2026-11-24-10:10:10")));

  QCOMPARE(decisions.headerData(4, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Decisions"));
  store.modified_decisions = 1;
  QCOMPARE(decisions.headerData(4, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Decisions*"));
  QVERIFY(decisions.headerData(4, Qt::Horizontal, Qt::ToolTipRole)
              .toString()
              .contains(QStringLiteral("1 modified")));
}

void UiTest::useAsLocationDialogChoices() {
  ui::UseAsLocationDialog dialog(QStringLiteral("docs"),
                                 QStringLiteral("/src/"),
                                 QStringLiteral("/dst/"));
  auto buttons = dialog.findChildren<QPushButton*>();
  QStringList labels;
  for (QPushButton* button : buttons) labels.append(button->text());
  QVERIFY(labels.contains(QStringLiteral("Use as Both")));
  QVERIFY(!labels.contains(QStringLiteral("Use as Source")));
  QVERIFY(!labels.contains(QStringLiteral("Use as Destination")));
  QVERIFY(labels.contains(QStringLiteral("Cancel")));
  QVERIFY(dialog.updateFilters());
}

void UiTest::useAsLocationMakesComparisonStale() {
  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src/"));
  edits.at(1)->setText(QStringLiteral("/dst/"));

  QTimer::singleShot(0, [&window]() {
    auto* dialog = window.findChild<ui::UseAsLocationDialog*>();
    if (dialog == nullptr) return;
    QPushButton* both = nullptr;
    for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
      if (button->text() == QStringLiteral("Use as Both")) both = button;
    }
    if (both == nullptr) return;
    auto* filters = dialog->findChild<QCheckBox*>();
    if (filters != nullptr) filters->setChecked(true);
    both->click();
  });
  const bool invoked = QMetaObject::invokeMethod(
      &window, "useAsLocation", Q_ARG(QString, QStringLiteral("docs")));
  QVERIFY(invoked);
  QCOMPARE(edits.at(0)->text(), QStringLiteral("/src/docs/"));
  QCOMPARE(edits.at(1)->text(), QStringLiteral("/dst/docs/"));
  auto* status = window.findChild<QLineEdit*>("status-label");
  QVERIFY(status->text().startsWith(QStringLiteral("Stale comparison")));
}

void UiTest::syncIsDisabledOnStartup() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  QAbstractButton* syncButton = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QStringLiteral("Synchronize")) syncButton = button;
  }
  QVERIFY(syncButton != nullptr);
  QVERIFY(!syncButton->isEnabled());
  QAction* syncAction = nullptr;
  for (QAction* action : window.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("Synchronize")) syncAction = action;
  }
  QVERIFY(syncAction != nullptr);
  QVERIFY(!syncAction->isEnabled());
}

void UiTest::invertedClicksCoverStateCombinations() {
  ui::ActionFilterBar bar;
  const auto box = [&bar](int number) {
    return bar.findChild<QCheckBox*>(QStringLiteral("filter-box-%1").arg(number));
  };
  const auto ctrlClick = [](QCheckBox* target) {
    QPointF point(5, 5);
    QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton,
                      Qt::LeftButton, Qt::ControlModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton,
                        Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(target, &press);
    QApplication::sendEvent(target, &release);
  };
  const auto allActions = [&bar]() {
    for (int action = 0; action < 5; ++action) {
      if (!bar.actionVisible(action)) return false;
    }
    return true;
  };
  QCheckBox* excluded = box(1);
  QCheckBox* create = box(3);
  QCheckBox* update = box(4);
  QVERIFY(excluded != nullptr && create != nullptr && update != nullptr);

  ctrlClick(create);
  QVERIFY(bar.actionVisible(0));
  for (int action = 1; action < 5; ++action) QVERIFY(!bar.actionVisible(action));
  ctrlClick(create);
  QVERIFY(allActions());

  // Ctrl+click on an action that is currently off restores all actions.
  ctrlClick(create);
  QVERIFY(!bar.actionVisible(1));
  ctrlClick(update);
  QVERIFY(allActions());

  // An action solo followed by ctrl+click excluded still selects only
  // excluded, and a second inverted click restores the default bar.
  ctrlClick(create);
  ctrlClick(excluded);
  QVERIFY(bar.showExcluded());
  QVERIFY(!allActions());
  ctrlClick(excluded);
  QVERIFY(!bar.showExcluded());
  QVERIFY(allActions());

  // A plain click still toggles, and reset clears the auxiliary toggles.
  QVERIFY(create->isChecked());
  QTest::mouseClick(create, Qt::LeftButton);
  QVERIFY(!bar.actionVisible(0));
  bar.toggleUnchanged();
  QVERIFY(bar.showUnchanged());
  bar.resetDefaults();
  QVERIFY(allActions());
  QVERIFY(!bar.showUnchanged());
  QVERIFY(!bar.showExcluded());
}

void UiTest::pathFiltersAndGlobsMatchVisibleRows() {
  QVector<core::Change> changes;
  const auto add = [&changes](const QString& path, core::Action action,
                              core::Kind kind) {
    core::Change change;
    change.path = path;
    change.action = action;
    change.kind = kind;
    change.modifiedTime = QStringLiteral("2026-11-23-11:23:22");
    changes.append(change);
  };
  add(QStringLiteral("docs/a.txt"), core::Action::UpdateContent, core::Kind::File);
  add(QStringLiteral("docs/sub/b.txt"), core::Action::UpdateContent, core::Kind::File);
  add(QStringLiteral("docs.txt"), core::Action::UpdateContent, core::Kind::File);
  add(QStringLiteral("pics/c.jpg"), core::Action::UpdateContent, core::Kind::File);

  auto visiblePaths = [](const model::DataStore& store) {
    QStringList paths;
    for (int id : store.visible) {
      if (id >= 0 && id < static_cast<int>(store.paths.size())) {
        paths.append(QString::fromStdString(store.paths[id]));
      }
    }
    return paths;
  };
  const auto build = [&changes](const QString& pattern, bool glob,
                                bool dirsOnly) {
    model::DataStore store;
    qt_rsync::model::loadChanges(store, changes);
    store.pattern = pattern.toLower().toStdString();
    store.pattern_glob = glob;
    store.dirs_only = dirsOnly;
    store.rebuild_visible();
    return store;
  };

  auto store = build(QStringLiteral("docs/"), false, false);
  auto paths = visiblePaths(store);
  QVERIFY(paths.contains(QStringLiteral("docs/a.txt")));
  QVERIFY(paths.contains(QStringLiteral("docs/sub/b.txt")));
  QVERIFY(!paths.contains(QStringLiteral("docs.txt")));
  QVERIFY(!paths.contains(QStringLiteral("pics/c.jpg")));

  store = build(QStringLiteral("docs"), false, false);
  paths = visiblePaths(store);
  QVERIFY(paths.contains(QStringLiteral("docs.txt")));

  store = build(QStringLiteral("*.jpg"), true, false);
  paths = visiblePaths(store);
  QVERIFY(paths.contains(QStringLiteral("pics/c.jpg")));
  QVERIFY(!paths.contains(QStringLiteral("docs/a.txt")));

  store = build(QString(), false, true);
  paths = visiblePaths(store);
  QVERIFY(paths.contains(QStringLiteral("docs")));
  QVERIFY(!paths.contains(QStringLiteral("docs/a.txt")));
}

void UiTest::startupActionStatesMatchPython() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  const auto findAction = [&window](const QString& text) {
    for (QAction* action : window.findChildren<QAction*>()) {
      if (action->text() == text) return action;
    }
    return static_cast<QAction*>(nullptr);
  };
  QAction* undo = findAction(QStringLiteral("Undo Decision"));
  QAction* redo = findAction(QStringLiteral("Redo Decision"));
  QVERIFY(undo != nullptr && redo != nullptr);
  QVERIFY(!undo->isEnabled());
  QVERIFY(!redo->isEnabled());
  QAction* verify = findAction(QStringLiteral("Verify Decisions"));
  QVERIFY(verify != nullptr);
  QVERIFY(!verify->isEnabled());
  QVERIFY(findAction(QStringLiteral("Synchronize")) != nullptr);
  QVERIFY(!findAction(QStringLiteral("Synchronize"))->isEnabled());
}

void UiTest::journalSeverityColoursMatchPython() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  QPlainTextEdit* journal = nullptr;
  for (QPlainTextEdit* edit : window.findChildren<QPlainTextEdit*>()) {
    if (edit->objectName() != QStringLiteral("command-preview-label")) journal = edit;
  }
  QVERIFY(journal != nullptr);
  journal->clear();
  const auto append = [&window](const QString& line) {
    QVERIFY(QMetaObject::invokeMethod(
        &window, "appendJournalLine", Q_ARG(QString, line)));
  };
  append(QStringLiteral("warning message"));
  append(QStringLiteral("error message"));
  append(QStringLiteral("comparison complete"));
  append(QStringLiteral("$ rsync --dry-run -- /src/ /dst/"));
  const auto colourOf = [journal](int row) {
    QTextBlock block = journal->document()->findBlockByNumber(row);
    QTextBlock::iterator it = block.begin();
    QString colour;
    while (!it.atEnd()) {
      const QTextFragment fragment = it.fragment();
      if (fragment.isValid() && !fragment.text().trimmed().isEmpty()) {
        colour = fragment.charFormat().foreground().color().name().toLower();
      }
      ++it;
    }
    return colour;
  };
  QVERIFY(!colourOf(0).isEmpty());
  QVERIFY(colourOf(0) != colourOf(1));
  QCOMPARE(colourOf(3), journal->palette().color(QPalette::Text).name().toLower());
}

void UiTest::staleRootTooltipAndContextMenuActions() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("docs/a.txt"),
                               QStringLiteral("docs/b.txt")}));
  view.setStale(true);
  auto& store = view.store();
  const auto rootIt = store.path_to_id.find(model::DataStore::root_path());
  QVERIFY(rootIt != store.path_to_id.end());
  model::TableModel sourceModel(&store, 0);
  const QModelIndex rootIndex =
      sourceModel.index(store.row_of_id.at(rootIt->second), 0);
  QVERIFY(sourceModel.data(rootIndex, Qt::ToolTipRole)
              .toString()
              .contains(QStringLiteral("stale comparison")));
  view.setStale(false);

  auto* sourceView = view.findChildren<QTableView*>().first();
  QVERIFY(sourceView != nullptr);
  view.show();
  QTest::qWait(10);
  QStringList labels;
  QTimer::singleShot(0, [&labels]() {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr) return;
    for (QAction* action : menu->actions()) labels.append(action->text());
    menu->close();
  });
  QVERIFY(QMetaObject::invokeMethod(
      &view, "showContextMenu", Q_ARG(int, 0), Q_ARG(QPoint, QPoint(30, 30))));
  for (const QString& expected : {QStringLiteral("Copy Path"),
                                  QStringLiteral("Copy Filename"),
                                  QStringLiteral("Copy Relative Path"),
                                  QStringLiteral("Open Directory"),
                                  QStringLiteral("Use as Source/Destination..."),
                                  QStringLiteral("Exclude via Filter"),
                                  QStringLiteral("Delete on Destination"),
                                  QStringLiteral("Collapse / Expand")}) {
    QVERIFY2(labels.contains(expected), qPrintable(expected));
  }
}

void UiTest::dialogRoundTripsCoverPortedFields() {
  ui::SettingsDialog settings;
  auto* settingsTabs = settings.findChild<QTabWidget*>();
  QVERIFY(settingsTabs != nullptr);
  QStringList settingsTitles;
  for (int index = 0; index < settingsTabs->count(); ++index) {
    settingsTitles.append(settingsTabs->tabText(index));
  }
  QCOMPARE(settingsTitles,
           QStringList({QStringLiteral("General"), QStringLiteral("Logging"),
                        QStringLiteral("Tools"), QStringLiteral("Advanced")}));
  auto* settingsButtons = settings.findChild<QDialogButtonBox*>();
  QVERIFY(settingsButtons != nullptr);
  QVERIFY(settingsButtons->standardButtons() & QDialogButtonBox::Ok);
  QVERIFY(settingsButtons->standardButtons() & QDialogButtonBox::Cancel);
  auto* sound = settings.findChild<QCheckBox*>();
  QVERIFY(sound != nullptr);
  const bool initialSound = sound->isChecked();
  sound->setChecked(!initialSound);
  QCOMPARE(sound->isChecked(), !initialSound);

  core::SyncProfile profile;
  profile.name = QStringLiteral("example");
  profile.source = core::parseEndpoint(QStringLiteral("/src/"));
  profile.destination = core::parseEndpoint(QStringLiteral("/dst/"));
  ui::ProfileEditorDialog editor(profile);
  auto* editorTabs = editor.findChild<QTabWidget*>();
  QVERIFY(editorTabs != nullptr);
  QStringList editorTitles;
  for (int index = 0; index < editorTabs->count(); ++index) {
    editorTitles.append(editorTabs->tabText(index));
  }
  QCOMPARE(editorTitles,
           QStringList({QStringLiteral("Options"), QStringLiteral("Preservation"),
                        QStringLiteral("Backup"), QStringLiteral("Filters")}));
  QCOMPARE(editor.profile().name, QStringLiteral("example"));

  QVector<core::Change> changes = fileChanges({QStringLiteral("a.txt")});
  core::Decisions decisions;
  const auto plan = core::buildExecutionPlan(profile, changes, decisions);
  ui::SyncDialog sync(profile, plan, QStringLiteral("/tmp/rules"));
  QCOMPARE(sync.windowTitle(), QStringLiteral("Confirm synchronization"));
  auto* syncButtons = sync.findChild<QDialogButtonBox*>();
  QVERIFY(syncButtons != nullptr);
  QVERIFY(syncButtons->button(QDialogButtonBox::Ok)->isEnabled());
  QCOMPARE(syncButtons->button(QDialogButtonBox::Ok)->text(),
           QStringLiteral("Synchronize Now"));
  const QString allText = sync.findChildren<QLabel*>().size() > 0
                              ? sync.findChildren<QLabel*>().first()->text()
                              : QString();
  QVERIFY(sync.findChildren<QPlainTextEdit*>().size() >= 2);
  QVERIFY(allText.contains(QStringLiteral("fingerprint")));
  auto* stats = sync.findChild<QTableWidget*>();
  QVERIFY(stats == nullptr ||
          stats->rowCount() >= 0);  // dialog uses a grid, not a table
}

void UiTest::goldenScreenshotsAreValidImages() {
  const QString root = QStringLiteral(QT_RSYNC_SOURCE_DIR);
  for (const QString& name : {QStringLiteral("main-window.png"),
                              QStringLiteral("settings-dialog.png"),
                              QStringLiteral("sync-dialog.png")}) {
    const QString path = root + QStringLiteral("/resources/") + name;
    QVERIFY2(QFile::exists(path), qPrintable(path));
    QPixmap pixmap(path);
    QVERIFY2(!pixmap.isNull(), qPrintable(path));
    QVERIFY(pixmap.width() > 100 && pixmap.height() > 100);
  }
}

void UiTest::exitSavePromptOffersSaveAs() {
  QWidget parent;
  QStringList labels;
  QTimer::singleShot(0, [&labels]() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* box = qobject_cast<QMessageBox*>(widget);
      if (box == nullptr || !box->isVisible()) continue;
      for (QAbstractButton* button : box->buttons()) labels.append(button->text());
      for (QAbstractButton* button : box->buttons()) {
        if (button->text() == QStringLiteral("Save As...")) {
          button->click();
          return;
        }
      }
    }
  });
  const auto choice =
      ui::MainWindow::askSaveProfile(&parent, QStringLiteral("Changed: Filters"), true);
  QCOMPARE(choice, ui::MainWindow::SaveChoice::SaveAs);
  QVERIFY(labels.contains(QStringLiteral("Save")));
  QVERIFY(labels.contains(QStringLiteral("Save As...")));
  QVERIFY(labels.contains(QStringLiteral("Don't Save")));
}

void UiTest::headerMenuTogglesExactSections() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt")}));
  auto tables = view.findChildren<QTableView*>();
  QCOMPARE(tables.size(), 3);

  const auto toggle = [&view](int pane, const QString& label) -> QPair<QStringList, QStringList> {
    QStringList labels;
    QStringList enabled;
    QTimer::singleShot(15, [&labels, &enabled, label]() {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (menu == nullptr) return;
      for (QAction* action : menu->actions()) {
        labels.append(action->text());
        if (action->isEnabled()) enabled.append(action->text());
        if (action->text() == label && action->isEnabled()) action->trigger();
      }
      menu->close();
    });
    if (!QMetaObject::invokeMethod(
            &view, "showHeaderMenu", Q_ARG(int, pane), Q_ARG(QPoint, QPoint(0, 0)))) {
      return qMakePair(labels, enabled);
    }
    return qMakePair(labels, enabled);
  };

  auto result = toggle(0, QStringLiteral("Items"));
  QVERIFY(result.first.contains(QStringLiteral("Item")));
  QVERIFY(result.second.contains(QStringLiteral("Items")));
  QVERIFY(tables[0]->isColumnHidden(1));
  toggle(0, QStringLiteral("Items"));
  QVERIFY(!tables[0]->isColumnHidden(1));

  // The decisions pane exposes only its Decisions column; the destination
  // pane maps the same menu entries onto its own column indices.
  for (int column : {0, 1, 2, 3, 5, 6, 7, 8, 9, 10}) {
    QVERIFY(tables[1]->isColumnHidden(column));
  }
  QVERIFY(!tables[1]->horizontalHeader()->isSectionHidden(4));
  QVERIFY(!tables[2]->horizontalHeader()->isSectionHidden(5));
  QVERIFY(tables[2]->horizontalHeader()->isSectionHidden(10));
}

void UiTest::contextMenuOpenActionsFollowEndpointKind() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("docs/a.txt")}));
  view.setRootNames(QStringLiteral("src-root"), QStringLiteral("dst-root"),
                    true, true);
  auto& store = view.store();
  const auto rootIt = store.path_to_id.find(model::DataStore::root_path());
  QVERIFY(rootIt != store.path_to_id.end());
  model::TableModel sourceModel(&store, 0);
  const QModelIndex rootIndex =
      sourceModel.index(store.row_of_id.at(rootIt->second), 0);
  QCOMPARE(sourceModel.data(rootIndex, Qt::DisplayRole).toString(),
           QStringLiteral("src-root"));
  model::TableModel destModel(&store, 2);
  const QModelIndex destRoot =
      destModel.index(store.row_of_id.at(rootIt->second), 0);
  QCOMPARE(destModel.data(destRoot, Qt::DisplayRole).toString(),
           QStringLiteral("dst-root"));

  const auto menuAt = [&view, &store](const QString& path) {
    QStringList labels;
    QTimer::singleShot(15, [&labels]() {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (menu == nullptr) return;
      for (QAction* action : menu->actions()) labels.append(action->text());
      menu->close();
    });
    const auto it = store.path_to_id.find(path.toStdString());
    const int row = it == store.path_to_id.end() ? 0 : store.row_of_id.at(it->second);
    QMetaObject::invokeMethod(&view, "showContextMenu", Q_ARG(int, 0),
                              Q_ARG(QPoint, QPoint(30, row * 22 + 11)));
    return labels;
  };

  QVERIFY(menuAt(QStringLiteral("docs/a.txt"))
              .contains(QStringLiteral("Open File")));
  QVERIFY(menuAt(QStringLiteral("docs"))
              .contains(QStringLiteral("Open Directory")));
  view.setRootNames(QStringLiteral("src-root"), QStringLiteral("dst-root"),
                    false, true);
  QVERIFY(!menuAt(QStringLiteral("docs"))
               .contains(QStringLiteral("Open Directory")));
  QVERIFY(!menuAt(QStringLiteral("docs/a.txt"))
               .contains(QStringLiteral("Open File")));
}

void UiTest::cellRolesAndPresenceMatchPython() {
  QVector<core::Change> changes;
  const auto add = [&changes](const QString& path, core::Action action,
                              core::Kind kind, qlonglong size) {
    core::Change change;
    change.path = path;
    change.action = action;
    change.kind = kind;
    change.size = size;
    change.modifiedTime = QStringLiteral("2026-11-23-11:23:22");
    changes.append(change);
  };
  add(QStringLiteral("new.txt"), core::Action::Create, core::Kind::File, 12345);
  add(QStringLiteral("gone.txt"), core::Action::Delete, core::Kind::File, 99);
  add(QStringLiteral("extra.txt"), core::Action::DestinationOnly,
      core::Kind::File, 77);
  add(QStringLiteral("link"), core::Action::UpdateContent,
      core::Kind::Symlink, 0);

  ui::ComparisonView view;
  view.setChanges(changes);
  auto& store = view.store();
  model::TableModel source(&store, 0);
  model::TableModel destination(&store, 2);
  const auto cell = [&store](model::TableModel& model, const QString& path,
                             int column) {
    const auto it = store.path_to_id.find(path.toStdString());
    if (it == store.path_to_id.end()) return QVariant();
    const int row = store.row_of_id.at(it->second);
    return model.data(model.index(row, column), Qt::DisplayRole);
  };
  QVERIFY(cell(source, QStringLiteral("new.txt"), 0).isValid());
  QVERIFY(!cell(destination, QStringLiteral("new.txt"), 0).isValid());
  QVERIFY(!cell(source, QStringLiteral("gone.txt"), 0).isValid());
  QVERIFY(cell(destination, QStringLiteral("gone.txt"), 0).isValid());
  QVERIFY(cell(destination, QStringLiteral("extra.txt"), 0).isValid());
  QCOMPARE(cell(source, QStringLiteral("new.txt"), 9).toLongLong(), 12345LL);
  QVERIFY(cell(source, QStringLiteral("new.txt"), 10).toString().isEmpty());

  const auto it = store.path_to_id.find(QStringLiteral("new.txt").toStdString());
  QVERIFY(it != store.path_to_id.end());
  const QModelIndex index = source.index(store.row_of_id.at(it->second), 0);
  QVERIFY(source.data(index, Qt::DecorationRole).isValid());
  QVERIFY(source.data(source.index(store.row_of_id.at(it->second), 4),
                      Qt::ToolTipRole)
              .toString()
              .contains(QStringLiteral("new")));
}

void UiTest::swapEndpointTexts() {
  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits[0]->setText(QStringLiteral("/src/"));
  edits[1]->setText(QStringLiteral("/dst/"));
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->accessibleName() == QStringLiteral("Swap Source and Destination")) {
      button->click();
      break;
    }
  }
  QCOMPARE(edits[0]->text(), QStringLiteral("/dst/"));
  QCOMPARE(edits[1]->text(), QStringLiteral("/src/"));
}

void UiTest::overviewFollowsActionFilters() {
  QVector<core::Change> changes = fileChanges(
      {QStringLiteral("a.txt"), QStringLiteral("b.txt")});
  changes[1].action = core::Action::UpdateContent;

  ui::ComparisonView view;
  view.setChanges(changes);
  ui::ActionFilterBar bar;
  view.setFilterBar(&bar);
  ui::OverviewPane overview;
  connect(&view, &ui::ComparisonView::viewChanged, &overview,
          [&overview, &view]() { overview.rebuildForDecision(view.store()); });
  overview.rebuild(view.store());
  auto* summary = overview.findChild<QLabel*>(QStringLiteral("statistics-summary"));
  QVERIFY(summary != nullptr);
  QVERIFY(summary->text().startsWith(QStringLiteral("2 item(s)")));

  auto* update = bar.findChild<QCheckBox*>(QStringLiteral("filter-box-4"));
  QVERIFY(update != nullptr);
  QTest::mouseClick(update, Qt::LeftButton);
  QVERIFY(!bar.actionVisible(1));
  QVERIFY(summary->text().startsWith(QStringLiteral("1 item(s)")));
}

void UiTest::contextMenuOffersExternalTools() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("docs/a.txt")}));
  core::ExternalTool tool;
  tool.name = QStringLiteral("Kate");
  tool.command = QStringLiteral("kate %d/%f");
  tool.enabled = true;
  view.setExternalTools({tool});

  QStringList submenuLabels;
  QTimer::singleShot(15, [&submenuLabels]() {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr) return;
    for (QMenu* submenu : menu->findChildren<QMenu*>()) {
      if (submenu->title() != QStringLiteral("Open with")) continue;
      for (QAction* action : submenu->actions()) {
        submenuLabels.append(action->text());
        QVERIFY2(!action->shortcut().isEmpty(), qPrintable(action->text()));
      }
    }
    menu->close();
  });
  QVERIFY(QMetaObject::invokeMethod(&view, "showContextMenu", Q_ARG(int, 0),
                                    Q_ARG(QPoint, QPoint(30, 30))));
  QVERIFY(submenuLabels.contains(QStringLiteral("Kate")));
}

void UiTest::foldingRefreshesViewsAndOverview() {
  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("docs/a.txt"),
                               QStringLiteral("docs/b.txt")}));
  const int before = static_cast<int>(view.store().visible.size());
  const auto docs = view.store().path_to_id.find(QStringLiteral("docs").toStdString());
  QVERIFY(docs != view.store().path_to_id.end());
  int changes = 0;
  connect(&view, &ui::ComparisonView::viewChanged, [&changes]() { ++changes; });
  view.selectPath(QStringLiteral("docs"));
  view.foldCurrent(0, true);
  QCOMPARE(changes, 1);
  QVERIFY(view.store().is_collapsed(docs->second));
  QVERIFY(static_cast<int>(view.store().visible.size()) < before);
  view.foldCurrent(0, false);
  QCOMPARE(changes, 2);
  QVERIFY(!view.store().is_collapsed(docs->second));
}

void UiTest::settingsUsePlainTextFormats() {
  const QList<int> values{100, 200, 300, 400};
  QCOMPARE(core::storage::decodeIntList(core::storage::encodeIntList(values)),
           values);
  QCOMPARE(core::storage::actionColourKeys().first(),
           QStringLiteral("create"));

  ui::MainWindow window;
  window.resize(900, 700);
  window.close();
  core::storage::Settings settings;
  const QStringList keys = settings.allKeys();
  QVERIFY(keys.contains(QStringLiteral("main-window/geometry")));
  QVERIFY(!keys.contains(QStringLiteral("main-window/state")));
  const QList<int> geometry = core::storage::decodeIntList(
      settings.value(QStringLiteral("main-window/geometry")).toString());
  QCOMPARE(geometry.size(), 4);
  QVERIFY(!keys.join(' ').contains(QStringLiteral("b64:")));
}

void UiTest::startupControlsAndDialogDefaults() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  const auto buttonByText = [&window](const QString& text) -> QAbstractButton* {
    for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
      if (button->text() == text) return button;
    }
    return nullptr;
  };
  QAbstractButton* cancel = buttonByText(QStringLiteral("Cancel"));
  QAbstractButton* pause = buttonByText(QStringLiteral("Pause"));
  QVERIFY(cancel != nullptr && pause != nullptr);
  QVERIFY(!cancel->isEnabled());
  QVERIFY(!pause->isEnabled());
  const QList<QTableView*> tables = window.findChildren<QTableView*>();
  QVERIFY(tables.size() == 3);
  QVERIFY(tables[1]->horizontalHeader()->styleSheet().contains(
      QStringLiteral("border")));

  int rightClickHints = 0;
  for (QToolButton* button : window.findChildren<QToolButton*>()) {
    if (button->toolTip().contains(QStringLiteral("Right-click"))) {
      ++rightClickHints;
    }
  }
  QVERIFY(rightClickHints >= 3);

  QMenu* profileMenu = nullptr;
  for (QToolButton* button : window.findChildren<QToolButton*>()) {
    if (button->accessibleName() == QStringLiteral("Manage Profiles")) {
      profileMenu = button->menu();
    }
  }
  QVERIFY(profileMenu != nullptr);
  QMetaObject::invokeMethod(profileMenu, "aboutToShow");
  QStringList labels;
  for (QAction* action : profileMenu->actions()) labels.append(action->text());
  QVERIFY(labels.indexOf(QStringLiteral("Edit...")) <
          labels.indexOf(QStringLiteral("Save As...")));

  core::SyncProfile profile;
  profile.source = core::parseEndpoint(QStringLiteral("/src/"));
  profile.destination = core::parseEndpoint(QStringLiteral("/dst/"));
  core::Decisions decisions;
  const auto plan = core::buildExecutionPlan(profile, {}, decisions);
  ui::SyncDialog dialog(profile, plan, QStringLiteral("/tmp/rules"));
  QVERIFY(dialog.compareAfterSync());
}

void UiTest::commandPreviewIsTemporaryJournalEntry() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  auto* progress = window.findChild<QProgressBar*>();
  QVERIFY(progress != nullptr);
  QVERIFY(qobject_cast<QStatusBar*>(progress->parentWidget()) != nullptr);
  QVERIFY(progress->width() >= window.width() * 15 / 100 - 4);
  QVERIFY(window.findChild<QPlainTextEdit*>(QStringLiteral("command-preview-label")) ==
          nullptr);

  auto* journal = window.findChild<QPlainTextEdit*>();
  QVERIFY(journal != nullptr);
  QVERIFY(!journal->toPlainText().contains(QStringLiteral("rsync preview:")));
  QAbstractButton* compare = nullptr;
  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QStringLiteral("Compare")) compare = button;
  }
  QVERIFY(compare != nullptr);
  QHoverEvent enter(QEvent::HoverEnter, QPointF(5, 5), QPointF());
  QApplication::sendEvent(compare, &enter);
  const QString text = journal->toPlainText();
  const QStringList lines = text.split('\n');
  QVERIFY(!lines.isEmpty());
  const QString last = lines.last().trimmed();
  QVERIFY2(last.startsWith(QStringLiteral("compare rsync preview: $ rsync")),
           qPrintable(last));
  QVERIFY(!last.contains(QRegularExpression(QStringLiteral("^\\d{2}:"))));

  // Repeated hovers replace the same single entry.
  QApplication::sendEvent(compare, &enter);
  QApplication::sendEvent(compare, &enter);
  QCOMPARE(journal->toPlainText().count(QStringLiteral("$ rsync")), 1);
  QVERIFY(!journal->toPlainText().split('\n').contains(QString()));

  // Any real journal entry removes the temporary preview first.
  QVERIFY(QMetaObject::invokeMethod(
      &window, "appendJournalLine", Q_ARG(QString, QStringLiteral("real line"))));
  QVERIFY(!journal->toPlainText().contains(QStringLiteral("$ rsync")));
  QVERIFY(journal->toPlainText().contains(QStringLiteral("real line")));

  QVERIFY(QMetaObject::invokeMethod(&window, "clearCommandPreview"));
  QVERIFY(!journal->toPlainText().contains(QStringLiteral("$ rsync --dry-run")));
}

void UiTest::overviewWidthStableAcrossLayouts() {
  ui::MainWindow window;
  window.resize(1280, 800);
  window.show();
  QTest::qWait(20);
  auto* overview = window.findChild<ui::OverviewPane*>();
  QVERIFY(overview != nullptr);
  const auto action = [&window](const QString& text) -> QAction* {
    for (QAction* candidate : window.findChildren<QAction*>()) {
      if (candidate->text() == text) return candidate;
    }
    return nullptr;
  };
  QAction* wide = action(QStringLiteral("Compare with overview"));
  QAction* normal = action(QStringLiteral("Normal"));
  QVERIFY(wide != nullptr && normal != nullptr);
  const int normalWidth = overview->width();
  QVERIFY(normalWidth > 0);

  wide->trigger();
  QTest::qWait(20);
  QCOMPARE(overview->width(), normalWidth);

  normal->trigger();
  QTest::qWait(20);
  QCOMPARE(overview->width(), normalWidth);
}

void UiTest::toolbarOrderAndLocationStaleness() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString script = dir.path() + QStringLiteral("/rsync");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("#!/bin/sh\n"
             "case \"$*\" in *--list-only*) exit 0 ;; esac\n"
             "printf '%s\\n' "
             "'>f+++++++++|qpy-rsync|1|qpy-rsync|2026-11-23|qpy-rsync|a.txt'\n");
  file.close();
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", (dir.path() + ':' + QString::fromLocal8Bit(oldPath)).toLocal8Bit());

  ui::MainWindow window;
  app::StartupOptions options;
  window.applyStartupOptions(options);
  const auto edits = QList<QLineEdit*>{window.findChild<QLineEdit*>("source-edit"),
                                       window.findChild<QLineEdit*>("destination-edit")};
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  window.show();

  const auto byAccessibleName = [&window](const QString& name) -> QToolButton* {
    for (QToolButton* button : window.findChildren<QToolButton*>()) {
      if (button->accessibleName() == name) return button;
    }
    return nullptr;
  };
  QToolButton* settings = byAccessibleName(QStringLiteral("Settings"));
  QToolButton* layout = byAccessibleName(QStringLiteral("Compare layout"));
  QToolButton* swap = byAccessibleName(QStringLiteral("Swap Source and Destination"));
  QVERIFY(settings != nullptr && layout != nullptr && swap != nullptr);
  QVERIFY(settings->x() < layout->x());
  QVERIFY(swap->x() < window.findChild<QComboBox*>()->x() + 200);
  QVERIFY(byAccessibleName(QStringLiteral("Find next")) == nullptr);
  QToolButton* backup = byAccessibleName(QStringLiteral("Open the backup directory"));
  QToolButton* log = byAccessibleName(QStringLiteral("Open the log file"));
  QVERIFY(backup != nullptr && log != nullptr);
  QCOMPARE(backup->popupMode(), QToolButton::InstantPopup);
  QCOMPARE(backup->menu()->actions().first()->text(),
           QStringLiteral("Open the backup directory"));
  QCOMPARE(log->menu()->actions().first()->text(),
           QStringLiteral("Open the log file"));
  QCOMPARE(log->menu()->actions().at(1)->text(), QStringLiteral("Clear Log"));
  QVERIFY(swap->parentWidget() != edits.at(0)->parentWidget());

  for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLineEdit*>("status-label");
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 8000);

  swap->click();
  QVERIFY(status->text().contains(QStringLiteral("Stale comparison")));
  swap->click();
  QVERIFY(status->text().contains(QStringLiteral("Locations restored")));
  QCOMPARE(edits.at(0)->text(), QStringLiteral("/src/"));
  QCOMPARE(edits.at(1)->text(), QStringLiteral("/dst/"));
  qputenv("PATH", oldPath);
}

void UiTest::escapeClearsSearchAndSearchSelectionReplaces() {
  ui::MainWindow window;
  window.show();
  QTest::qWait(10);
  auto* search = window.findChild<QLineEdit*>(QStringLiteral("search-edit"));
  QVERIFY(search != nullptr);
  search->setText(QStringLiteral("needle"));
  QCOMPARE(search->text(), QStringLiteral("needle"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(search, &escape);
  QVERIFY(search->text().isEmpty());
  auto* status = window.findChild<QLabel*>(QStringLiteral("search-status"));
  QVERIFY(status == nullptr || !status->isVisible());

  ui::ComparisonView view;
  view.setChanges(fileChanges({QStringLiteral("a.txt"), QStringLiteral("b.txt"),
                               QStringLiteral("c.txt")}));
  view.selectPath(QStringLiteral("a.txt"));
  view.selectPath(QStringLiteral("b.txt"));
  auto* source = view.findChildren<QTableView*>().first();
  QCOMPARE(source->selectionModel()->selectedRows().size(), 1);
  const QModelIndex current = source->currentIndex();
  QVERIFY(current.isValid());
  QCOMPARE(source->model()->data(current, Qt::UserRole).toString(),
           QStringLiteral("b.txt"));
}

QTEST_MAIN(UiTest)
#include "test_ui.moc"
