#include "ui/ActionFilterBar.h"

#include <QCheckBox>
#include <QLabel>
#include <QToolButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QShortcut>
#include <QToolTip>

#include "ui/Icons.h"

namespace qt_rsync::ui {

ActionFilterBar::ActionFilterBar(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(1);

  const auto separator = [&]() {
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedWidth(1);
    layout->addWidget(line);
  };
  const auto check = [&](const QString& tooltip, const QIcon& icon,
                         int shortcutNumber, bool checked = true) {
    auto* box = new QCheckBox(this);
    box->setIcon(icon);
    box->setToolTip(tooltip);
    box->setAccessibleName(tooltip);
    box->setChecked(checked);
    box->setFixedSize(44, 40);
    layout->addWidget(box);
    connect(box, &QCheckBox::toggled, this, &ActionFilterBar::filterChanged);
    if (shortcutNumber > 0) {
      auto* alt = new QShortcut(
          QKeySequence(Qt::ALT | (Qt::Key_0 + shortcutNumber)), this);
      connect(alt, &QShortcut::activated, box, [box]() { box->click(); });
      auto* ctrl = new QShortcut(
          QKeySequence(Qt::CTRL | (Qt::Key_0 + shortcutNumber)), this);
      connect(ctrl, &QShortcut::activated, this, [this, box]() {
        for (QCheckBox* other : findChildren<QCheckBox*>()) {
          if (other != box) other->setChecked(false);
        }
        box->setChecked(true);
      });
    }
    return box;
  };

  excluded_ = check(QStringLiteral("Show excluded items"),
                    icons::toolbarIcon(QStringLiteral("undo")), 1, false);
  separator();
  directories_ = check(
      QStringLiteral("Apply filters to directories too"), icons::folderIcon(), 0,
      false);
  separator();
  const QVector<QPair<QString, int>> actionLabels{
      {QStringLiteral("Create"), 0},
      {QStringLiteral("Update"), 1},
      {QStringLiteral("Metadata only"), 2},
      {QStringLiteral("Delete"), 3},
      {QStringLiteral("Keep in Destination"), 4},
  };
  int number = 2;
  for (const auto& entry : actionLabels) {
    actions_.append(check(
        QStringLiteral("Show %1 items").arg(entry.first),
        icons::actionIcon(entry.second), number++));
    separator();
  }
  unchanged_ = check(QStringLiteral("Show unchanged items"),
                     icons::toolbarIcon(QStringLiteral("search")), 8, false);
  connect(unchanged_, &QCheckBox::toggled, this,
          &ActionFilterBar::unchangedToggled);
  separator();
  conflicts_ = check(QStringLiteral("Review changed decisions"),
                     icons::toolbarIcon(QStringLiteral("verify")), 0, false);
  conflicts_->hide();
  excludedCount_ = new QLabel(QStringLiteral("0"), this);
  excludedCount_->setToolTip(QStringLiteral("Excluded items"));
  layout->addWidget(excludedCount_);
  undo_ = new QToolButton(this);
  undo_->setText(QStringLiteral("Undo"));
  undo_->setIcon(icons::toolbarIcon(QStringLiteral("undo")));
  undo_->setToolTip(QStringLiteral("Undo Decision (Ctrl+Z)"));
  undo_->setEnabled(false);
  layout->addWidget(undo_);
  connect(undo_, &QToolButton::clicked, this, &ActionFilterBar::undoRequested);
}

bool ActionFilterBar::showExcluded() const { return excluded_->isChecked(); }
bool ActionFilterBar::filterDirectories() const {
  return directories_->isChecked();
}
bool ActionFilterBar::actionVisible(int action) const {
  return action >= 0 && action < actions_.size() &&
         actions_.at(action)->isChecked();
}
bool ActionFilterBar::showConflicts() const { return conflicts_->isChecked(); }

void ActionFilterBar::setExcludedCount(int count) {
  excludedCount_->setText(QString::number(count));
}

void ActionFilterBar::setUndoEnabled(bool enabled) {
  undo_->setEnabled(enabled);
}

void ActionFilterBar::setConflictsAvailable(bool available) {
  conflicts_->setVisible(available);
}

}  // namespace qt_rsync::ui
