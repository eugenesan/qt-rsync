#include <QtTest>

#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QTimer>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QTabWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTableView>

#include "ui/MainWindow.h"
#include "app/StartupOptions.h"
#include "ui/ComparisonView.h"
#include "ui/SettingsDialog.h"
#include "core/Storage.h"
#include "core/Paths.h"

using namespace qt_rsync;

class UiTest : public QObject {
  Q_OBJECT

 private slots:
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
};

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

QTEST_MAIN(UiTest)
#include "test_ui.moc"
