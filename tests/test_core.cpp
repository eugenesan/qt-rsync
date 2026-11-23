#include <QtTest>

#include "core/Decision.h"
#include "core/Profile.h"

class CoreTest : public QObject {
  Q_OBJECT

 private slots:
  void decisionsAreNotSelectableForUnchangedRows();
  void endpointOperandKeepsSshSyntax();
};

void CoreTest::decisionsAreNotSelectableForUnchangedRows() {
  qt_rsync::core::Change change;
  change.action = qt_rsync::core::Action::NoChange;
  qt_rsync::core::Decisions decisions;
  QVERIFY(!decisions.isSelectable(change));
  QCOMPARE(decisions.effective(change), qt_rsync::core::Decision::Exclude);
}

void CoreTest::endpointOperandKeepsSshSyntax() {
  qt_rsync::core::Endpoint endpoint;
  endpoint.kind = qt_rsync::core::EndpointKind::Ssh;
  endpoint.host = QStringLiteral("example");
  endpoint.path = QStringLiteral("/home/user/src");
  QCOMPARE(endpoint.rsyncOperand(), QStringLiteral("example:/home/user/src"));
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
