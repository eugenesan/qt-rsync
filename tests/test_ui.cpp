#include <QtTest>

#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QAbstractButton>
#include <QCheckBox>
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

using namespace qt_rsync;

class UiTest : public QObject {
  Q_OBJECT

 private slots:
  void compareRendersFakeRsyncOutput();
  void spaceExcludesEverySelectedItem();
  void settingsDialogPersistsDiagnostics();
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

QTEST_MAIN(UiTest)
#include "test_ui.moc"
