#include "ui/SyncDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/Command.h"

namespace qt_rsync::ui {
namespace {

QString formatSize(std::int64_t bytes) {
  if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
  if (bytes < 1024 * 1024) {
    return QString::number(bytes / 1024.0, 'f', 1) +
           QStringLiteral(" KiB");
  }
  return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) +
         QStringLiteral(" MiB");
}

}  // namespace

SyncDialog::SyncDialog(const core::SyncProfile& profile,
                       const core::ExecutionPlan& plan,
                       const QString& filterFilePath, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Confirm synchronization"));
  resize(720, 700);
  auto* layout = new QVBoxLayout(this);

  const auto& summary = plan.summary;
  auto* heading = new QLabel(
      QStringLiteral("Review selected synchronization (fingerprint: %1)")
          .arg(plan.itemsFingerprint.left(12)),
      this);
  heading->setStyleSheet(QStringLiteral("font-weight: bold;"));
  heading->setWordWrap(true);
  layout->addWidget(heading);

  auto* details = new QFormLayout();
  details->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  details->addRow(QStringLiteral("Source"), new QLabel(profile.source.path, this));
  details->addRow(QStringLiteral("Destination"),
                  new QLabel(profile.destination.path, this));
  details->addRow(
      QStringLiteral("Backup"),
      new QLabel(profile.backup.directory.isEmpty()
                     ? QStringLiteral("Disabled")
                     : profile.backup.directory,
                 this));
  details->addRow(QStringLiteral("Nested protection"),
                  new QLabel(QStringLiteral("Not required"), this));
  layout->addLayout(details);

  auto* grid = new QGridLayout();
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(3, 1);
  const auto addCell = [&](int row, int column, const QString& text) {
    grid->addWidget(new QLabel(text, this), row, column);
  };
  addCell(0, 0, QStringLiteral("Included"));
  addCell(0, 1, QString::number(summary.included));
  addCell(0, 2, QStringLiteral("Create"));
  addCell(0, 3, QString::number(summary.creates));
  addCell(1, 0, QStringLiteral("Total size"));
  addCell(1, 1, formatSize(summary.includedSize));
  addCell(1, 2, QStringLiteral("Update"));
  addCell(1, 3, QString::number(summary.updates));
  addCell(2, 0, QStringLiteral("Unchecked"));
  addCell(2, 1, QString::number(summary.excluded));
  addCell(2, 2, QStringLiteral("Metadata"));
  addCell(2, 3, QString::number(summary.metadata));
  addCell(3, 0, QStringLiteral("Retained in Destination"));
  addCell(3, 1, QString::number(summary.retainedDestinationOnly));
  addCell(3, 2, QStringLiteral("Delete"));
  addCell(3, 3, QString::number(summary.deletions));
  layout->addLayout(grid);

  auto* warning = new QLabel(
      summary.destructive > 0
          ? QStringLiteral(
                "This plan contains %1 destructive operation(s). Destination "
                "files will be removed.")
                .arg(summary.destructive)
          : QStringLiteral("No destructive operations are selected."),
      this);
  warning->setStyleSheet(
      summary.destructive > 0
          ? QStringLiteral("color: #B91C1C; font-weight: bold;")
          : QStringLiteral("color: #15803D; font-weight: bold;"));
  warning->setWordWrap(true);
  layout->addWidget(warning);

  auto* commandLabel = new QLabel(QStringLiteral("rsync command"), this);
  commandLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(commandLabel);
  auto* command = new QPlainTextEdit(this);
  command->setReadOnly(true);
  command->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  command->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  command->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  command->setMaximumHeight(104);
  command->setPlainText(
      core::buildSyncSpec(profile, filterFilePath).argv().join(' '));
  command->setToolTip(command->toPlainText());
  layout->addWidget(command);

  auto* rulesLabel = new QLabel(
      QStringLiteral("Decision rules (%1)")
          .arg(QFileInfo(filterFilePath).fileName()),
      this);
  rulesLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(rulesLabel);
  auto* rules = new QPlainTextEdit(this);
  rules->setReadOnly(true);
  rules->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  rules->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  rules->setMaximumHeight(104);
  const QString ruleText = core::decisionFilterText(plan);
  rules->setPlainText(ruleText);
  rules->setToolTip(ruleText);
  layout->addWidget(rules);
  layout->addStretch(1);

  auto* confirmation = new QLabel(
      QStringLiteral(
          "Confirming will launch write-capable rsync for the reviewed Source "
          "and Destination. The private decision filter is created for this "
          "confirmation and removed after execution."),
      this);
  confirmation->setStyleSheet(QStringLiteral("font-weight: bold;"));
  confirmation->setWordWrap(true);
  layout->addWidget(confirmation);

  compareAfter_ = new QCheckBox(
      QStringLiteral("Compare again after synchronization"), this);
  compareAfter_->setChecked(true);
  compareAfter_->setToolTip(QStringLiteral(
      "Run a fresh dry run as soon as the transfer completes so the "
      "comparison and overview reflect the new state."));
  layout->addWidget(compareAfter_);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)
      ->setText(QStringLiteral("Synchronize Now"));
  connect(buttons, &QDialogButtonBox::accepted, this, &SyncDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &SyncDialog::reject);
  layout->addWidget(buttons);
}

bool SyncDialog::compareAfterSync() const {
  return compareAfter_ != nullptr && compareAfter_->isChecked();
}

}  // namespace qt_rsync::ui
