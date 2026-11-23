#include <QtTest>

#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTableView>

#include "ui/MainWindow.h"

using namespace qt_rsync;

class UiTest : public QObject {
  Q_OBJECT

 private slots:
  void compareRendersFakeRsyncOutput();
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

QTEST_MAIN(UiTest)
#include "test_ui.moc"
