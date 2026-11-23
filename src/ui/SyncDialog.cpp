#include "ui/SyncDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/Command.h"

namespace qt_rsync::ui {

SyncDialog::SyncDialog(const core::SyncProfile& profile,
                       const core::ExecutionPlan& plan,
                       const QString& filterFilePath, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Synchronize"));
  resize(820, 560);
  auto* layout = new QVBoxLayout(this);

  const auto& summary = plan.summary;
  auto* heading = new QLabel(
      QStringLiteral("Selected: %1 item(s) — new %2, updated %3, metadata %4, "
                     "deleted %5; kept %6, destructive %7")
          .arg(summary.included)
          .arg(summary.creates)
          .arg(summary.updates)
          .arg(summary.metadata)
          .arg(summary.deletions)
          .arg(summary.retainedDestinationOnly)
          .arg(summary.destructive), this);
  heading->setWordWrap(true);
  layout->addWidget(heading);

  if (summary.destructive > 0) {
    auto* warning = new QLabel(
        QStringLiteral("This plan contains %1 destructive operation(s). "
                       "Destination files will be removed.")
            .arg(summary.destructive), this);
    warning->setStyleSheet(QStringLiteral("color: #B91C1C; font-weight: bold;"));
    warning->setWordWrap(true);
    layout->addWidget(warning);
  }

  auto* tabs = new QTabWidget(this);
  auto* command = new QPlainTextEdit(tabs);
  command->setReadOnly(true);
  command->setPlainText(
      core::buildSyncSpec(profile, filterFilePath).argv().join(' '));
  tabs->addTab(command, QStringLiteral("Command"));

  auto* rules = new QPlainTextEdit(tabs);
  rules->setReadOnly(true);
  rules->setPlainText(core::decisionFilterText(plan));
  tabs->addTab(rules, QStringLiteral("Decision rules"));
  layout->addWidget(tabs, 1);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Synchronize"));
  connect(buttons, &QDialogButtonBox::accepted, this, &SyncDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &SyncDialog::reject);
  layout->addWidget(buttons);
}

}  // namespace qt_rsync::ui
