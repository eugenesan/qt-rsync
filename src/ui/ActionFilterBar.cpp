#include "ui/ActionFilterBar.h"

#include <QCheckBox>
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
  layout->setSpacing(4);

  const auto separator = [&]() {
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedWidth(1);
    layout->addWidget(line);
  };
  int boxCounter = 0;
  const auto check = [&](const QString& tooltip, const QIcon& icon,
                         int shortcutNumber, bool checked = true) {
    auto* box = new QCheckBox(this);
    box->setIcon(icon);
    box->setIconSize(QSize(24, 24));
    box->setToolTip(tooltip);
    box->setAccessibleName(tooltip);
    box->setObjectName(QStringLiteral("filter-box-%1").arg(++boxCounter));
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

  excluded_ = check(
      QStringLiteral(
          "Show the items excluded from this comparison.\n"
          "An excluded item is not part of the sync. Exclusions are kept in "
          "memory only, so they are gone after the next comparison.\n"
          "Right-click to reset all exclusions.\n"
          "Ctrl+click shows only the excluded items.\n"
          "Alt+1 clicks this checkbox; Ctrl+1 inverts it."),
      icons::toolbarIcon(QStringLiteral("excluded")), 1, false);
  excluded_->setAccessibleName(QStringLiteral("Show excluded items"));
  separator();
  directories_ = check(
      QStringLiteral(
          "Apply the action filters to reported directories too.\n"
          "A directory whose own action is filtered out stays as a faded "
          "parent when it holds visible items, and is not counted."),
      icons::folderIcon(), 0, false);
  directories_->setAccessibleName(
      QStringLiteral("Apply filters to directories"));
  separator();
  const QVector<QPair<QString, int>> actionLabels{
      {QStringLiteral("Create"), 0},
      {QStringLiteral("Update"), 1},
      {QStringLiteral("Metadata only"), 2},
      {QStringLiteral("Delete"), 3},
      {QStringLiteral("Keep in Destination"), 4},
  };
  int number = 2;
  for (int index = 0; index < actionLabels.size(); ++index) {
    const auto& entry = actionLabels.at(index);
    const int shortcutNumber = number++;
    auto* box = check(
        QStringLiteral(
            "Display only: %1\n"
            "This checkbox filters the view and doesn't affect the sync. Use "
            "the sync mode selector or the action checkboxes below to control "
            "the sync.\n"
            "Alt+%2 clicks this checkbox; Ctrl+%2 inverts it.\n"
            "Ctrl+click shows only this action, and a second inverted click "
            "brings every action back. Right-click opens the same choices.")
            .arg(entry.first)
            .arg(shortcutNumber),
        icons::actionIcon(entry.second), shortcutNumber);
    box->setAccessibleName(
        QStringLiteral("Show %1 items").arg(entry.first));
    actions_.append(box);
    if (index < actionLabels.size() - 1) separator();
  }
  separator();
  unchanged_ = check(
      QStringLiteral(
          "Show the unchanged items of the comparison.\n"
          "The inventory lists both sides, so the unchanged items can be "
          "shown next to the changed ones.\n"
          "Ctrl+click shows only the unchanged items.\n"
          "Alt+8 clicks this checkbox; Ctrl+8 inverts it.\n\n"
          "Shortcut: F6"),
      icons::toolbarIcon(QStringLiteral("unchanged")), 8, false);
  unchanged_->setAccessibleName(QStringLiteral("Show unchanged items"));
  connect(unchanged_, &QCheckBox::toggled, this,
          &ActionFilterBar::unchangedToggled);
  separator();
  conflicts_ = check(
      QStringLiteral(
          "Show only the items whose decision no longer applies after a "
          "comparison — for example an item that became destination-only. "
          "Review them and choose the rsync default with the context menu."),
      icons::toolbarIcon(QStringLiteral("conflict")), 0, false);
  conflicts_->setAccessibleName(QStringLiteral("Review changed decisions"));
  conflicts_->hide();
  undo_ = new QToolButton(this);
  undo_->setObjectName(QStringLiteral("undo-button"));
  undo_->setText(QStringLiteral("Undo"));
  undo_->setIcon(icons::toolbarIcon(QStringLiteral("undo")));
  undo_->setIconSize(QSize(24, 24));
  undo_->setFixedSize(40, 40);
  undo_->setToolTip(QStringLiteral("Undo the last decision (Ctrl+Z)"));
  undo_->setAccessibleName(QStringLiteral("Undo decision"));
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

void ActionFilterBar::setActionColours(const QVector<QString>& colours) {
  for (int index = 0; index < actions_.size(); ++index) {
    if (index < colours.size() && !colours.at(index).isEmpty()) {
      actions_.at(index)->setIcon(
          icons::actionIcon(index, QColor(colours.at(index))));
    } else {
      actions_.at(index)->setIcon(icons::actionIcon(index));
    }
  }
}

void ActionFilterBar::setExcludedCount(int count) {
  // qpy puts the count on the Excluded checkbox itself, not in a separate
  // label next to the undo button.
  const QString text = count > 0 ? QString::number(count) : QString();
  excluded_->setText(text);
  const int extra =
      text.isEmpty() ? 0 : excluded_->fontMetrics().horizontalAdvance(text) + 10;
  excluded_->setFixedWidth(44 + extra);
  excluded_->setToolTip(
      (count > 0
           ? QStringLiteral("%1 excluded item(s).").arg(count)
           : QStringLiteral("Show the items excluded from this comparison.")) +
      QStringLiteral(
          "\nAn excluded item is not part of the sync. Exclusions are kept "
          "in memory only, so they are gone after the next comparison.\n"
          "Right-click to reset all exclusions.\n"
          "Ctrl+click shows only the excluded items.\n"
          "Alt+1 clicks this checkbox; Ctrl+1 inverts it."));
}

void ActionFilterBar::setUndoEnabled(bool enabled) {
  undo_->setEnabled(enabled);
}

void ActionFilterBar::setConflictsAvailable(bool available) {
  conflicts_->setVisible(available);
}

}  // namespace qt_rsync::ui
