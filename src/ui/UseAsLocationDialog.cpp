#include "ui/UseAsLocationDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace qt_rsync::ui {

UseAsLocationDialog::UseAsLocationDialog(const QString& relativePath,
                                         const QString& sourceRoot,
                                         const QString& destinationRoot,
                                         QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Use folder as Source or Destination"));
  resize(560, 260);

  auto* layout = new QVBoxLayout(this);
  auto* heading = new QLabel(
      QStringLiteral("Narrow the comparison to \"%1\"?").arg(relativePath), this);
  heading->setStyleSheet(QStringLiteral("font-weight: bold;"));
  heading->setWordWrap(true);
  layout->addWidget(heading);

  auto* details = new QFormLayout();
  const auto joined = [&relativePath](const QString& root) {
    QString base = root;
    while (base.endsWith('/')) base.chop(1);
    return base + QLatin1Char('/') + relativePath + QLatin1Char('/');
  };
  details->addRow(QStringLiteral("New Source"), new QLabel(joined(sourceRoot), this));
  details->addRow(QStringLiteral("New Destination"),
                  new QLabel(joined(destinationRoot), this));
  layout->addLayout(details);

  updateFilters_ = new QCheckBox(
      QStringLiteral("Update paths in the permanent filter rules"), this);
  updateFilters_->setChecked(true);
  updateFilters_->setToolTip(
      QStringLiteral("Remove the eliminated directory prefix from filters "
                     "that name paths inside it."));
  layout->addWidget(updateFilters_);

  auto* note = new QLabel(
      QStringLiteral("The current comparison will become stale: run Compare "
                     "again before synchronizing. The changed locations and "
                     "filters are not saved automatically — use Save or Save "
                     "As afterwards to persist the profile."),
      this);
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* buttons = new QDialogButtonBox(this);
  auto* source = buttons->addButton(QStringLiteral("Use as Source"),
                                    QDialogButtonBox::AcceptRole);
  auto* destination = buttons->addButton(QStringLiteral("Use as Destination"),
                                         QDialogButtonBox::AcceptRole);
  auto* both = buttons->addButton(QStringLiteral("Use as Both"),
                                  QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);

  connect(source, &QPushButton::clicked, this, [this]() {
    target_ = Target::Source;
    accept();
  });
  connect(destination, &QPushButton::clicked, this, [this]() {
    target_ = Target::Destination;
    accept();
  });
  connect(both, &QPushButton::clicked, this, [this]() {
    target_ = Target::Both;
    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool UseAsLocationDialog::updateFilters() const {
  return updateFilters_ != nullptr && updateFilters_->isChecked();
}

}  // namespace qt_rsync::ui
