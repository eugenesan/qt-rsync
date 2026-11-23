#include <QtTest>

#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QAbstractButton>
#include <QCheckBox>
#include <QDialog>
#include <QTimer>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTableView>

#include "ui/MainWindow.h"
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
  const auto edits = window.findChildren<QLineEdit*>();
  QVERIFY(edits.size() >= 2);
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));

  QPushButton* compare = nullptr;
  for (QPushButton* button : window.findChildren<QPushButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
  }
  QVERIFY(compare != nullptr);
  compare->click();

  QLabel* status = window.findChild<QLabel*>();
  QVERIFY(status != nullptr);
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  QVERIFY(status->text().contains(QStringLiteral("3 item(s)")));
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
  const auto edits = window.findChildren<QLineEdit*>();
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QPushButton* button : window.findChildren<QPushButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLabel*>();
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
  qputenv("PATH", oldPath);
}

void UiTest::settingsDialogPersistsDiagnostics() {
  QTemporaryDir dir;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", dir.path().toUtf8());
  ui::SettingsDialog dialog;
  QCOMPARE(dialog.findChildren<QTabWidget*>().size(), 1);
  QCOMPARE(dialog.findChildren<QCheckBox*>().size(), 4);
  for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
    if (box->text().contains(QStringLiteral("timing"))) box->setChecked(true);
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
  const auto edits = window.findChildren<QLineEdit*>();
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  QPushButton* compare = nullptr;
  QPushButton* verify = nullptr;
  QPushButton* sync = nullptr;
  for (QPushButton* button : window.findChildren<QPushButton*>()) {
    if (button->text() == QLatin1String("Compare")) compare = button;
    if (button->text().startsWith(QLatin1String("Verify"))) verify = button;
    if (button->text().startsWith(QLatin1String("Synchronize"))) sync = button;
  }
  QVERIFY(compare && verify && sync);
  compare->click();
  auto* status = window.findChild<QLabel*>();
  QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Comparison complete")), 5000);
  QVERIFY(verify->isEnabled());
  QVERIFY(sync->isEnabled());

  verify->click();
  QTest::qWait(100);
  QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("Verification")), 5000);

  QTimer::singleShot(100, [&]() {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<QDialog*>(widget);
      if (dialog == nullptr || !dialog->isVisible() || dialog->windowTitle() != QLatin1String("Synchronize")) continue;
      for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text() == QLatin1String("Synchronize")) button->click();
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
  const auto edits = window.findChildren<QLineEdit*>();
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
  const auto edits = window.findChildren<QLineEdit*>();
  edits.at(0)->setText(QStringLiteral("/src"));
  edits.at(1)->setText(QStringLiteral("/dst"));
  for (QPushButton* button : window.findChildren<QPushButton*>()) {
    if (button->text() == QLatin1String("Compare")) button->click();
  }
  auto* status = window.findChild<QLabel*>();
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

QTEST_MAIN(UiTest)
#include "test_ui.moc"
