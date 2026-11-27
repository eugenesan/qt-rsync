#include "ui/ActionFilterBar.h"

#include <QMenu>
#include <QMouseEvent>
#include <QEvent>
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
    // Breeze underlines a focused icon-only checkbox. A pointer click must not
    // leave that marker behind; Tab still reaches the control.
    box->setFocusPolicy(Qt::TabFocus);
    layout->addWidget(box);
    connect(box, &QCheckBox::toggled, this, &ActionFilterBar::filterChanged);
    // Fallback for platforms that only expose the modifier on click().
    connect(box, &QCheckBox::clicked, this, [this, box](bool) {
      if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) {
        applyInverse(box);
      }
    });
    if (shortcutNumber > 0) {
      auto* alt = new QShortcut(
          QKeySequence(Qt::ALT | (Qt::Key_0 + shortcutNumber)), this);
      alt->setContext(Qt::ApplicationShortcut);
      connect(alt, &QShortcut::activated, box, [box]() { box->click(); });
      auto* ctrl = new QShortcut(
          QKeySequence(Qt::CTRL | (Qt::Key_0 + shortcutNumber)), this);
      ctrl->setContext(Qt::ApplicationShortcut);
      connect(ctrl, &QShortcut::activated, this, [this, box]() {
        // The pointer guard only guards one mouse press/release cycle.
        inverseHandled_ = false;
        applyInverse(box);
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
  excluded_->setObjectName(QStringLiteral("excluded-checkbox"));
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
            "the sync mode selector or the decision checkboxes below to control "
            "the sync.\n"
            "Alt+%2 clicks this checkbox; Ctrl+%2 inverts it.\n"
            "Ctrl+click shows only this decision, and a second inverted click "
            "brings every decision back. Right-click opens the same choices.")
            .arg(entry.first)
            .arg(shortcutNumber),
        icons::actionIcon(entry.second), shortcutNumber);
    box->setAccessibleName(
        QStringLiteral("Show %1 items").arg(entry.first));
    static const QStringList actionObjectNames{
        QStringLiteral("create-checkbox"), QStringLiteral("update-checkbox"),
        QStringLiteral("metadata-checkbox"), QStringLiteral("delete-checkbox"),
        QStringLiteral("keep-checkbox")};
    box->setObjectName(actionObjectNames.at(index));
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
  unchanged_->setObjectName(QStringLiteral("unchanged-checkbox"));
  connect(unchanged_, &QCheckBox::toggled, this,
          &ActionFilterBar::unchangedToggled);
  separator();
  directories_ = check(
      QStringLiteral(
          "Apply the decision filters to reported directories too.\n"
          "A directory whose own change is filtered out stays as a faded "
          "parent when it holds visible items, and is not counted.\n"
          "Alt+9 clicks this checkbox; Ctrl+9 inverts it."),
      icons::folderIcon(), 9, false);
  directories_->setAccessibleName(
      QStringLiteral("Apply filters to directories"));
  directories_->setObjectName(QStringLiteral("directories-checkbox"));
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
  undo_->setIcon(icons::toolbarIcon(QStringLiteral("edit-undo")));
  undo_->setIconSize(QSize(24, 24));
  undo_->setFixedSize(40, 40);
  undo_->setToolTip(QStringLiteral("Undo the last decision (Ctrl+Z)"));
  undo_->setAccessibleName(QStringLiteral("Undo decision"));
  undo_->setEnabled(false);
  // qpy puts Undo at the start of the decisions bar, after the separator that
  // follows the Source panel and before the Excluded toggle.
  layout->insertWidget(0, undo_);
  connect(undo_, &QToolButton::clicked, this, &ActionFilterBar::undoRequested);

  const auto all = allBoxes();
  for (QCheckBox* box : all) {
    box->installEventFilter(this);
    box->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(box, &QWidget::customContextMenuRequested, this,
            [this, box](const QPoint& point) {
              if (box == excluded_) {
                showExcludedMenu(point);
              } else {
                showBoxMenu(box, point);
              }
            });
  }
}

QVector<QCheckBox*> ActionFilterBar::allBoxes() const {
  QVector<QCheckBox*> boxes = actions_;
  boxes.append(unchanged_);
  boxes.append(excluded_);
  return boxes;
}

bool ActionFilterBar::eventFilter(QObject* watched, QEvent* event) {
  auto* box = qobject_cast<QCheckBox*>(watched);
  if (box == nullptr || !allBoxes().contains(box)) {
    return QWidget::eventFilter(watched, event);
  }
  if (event->type() == QEvent::MouseButtonPress) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    // A new physical click starts a new inverse cycle.
    inverseHandled_ = false;
    if (mouse->button() == Qt::LeftButton &&
        (mouse->modifiers() & Qt::ControlModifier)) {
      // Consume both press and release so the checkbox does not toggle
      // itself after the inverted state has already been applied.
      inverseClick_ = box;
      applyInverse(box);
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (inverseClick_ == box) {
      inverseClick_ = nullptr;
      return true;
    }
    // Some window managers deliver the modifier only with the release.
    if (mouse->button() == Qt::LeftButton &&
        (mouse->modifiers() & Qt::ControlModifier)) {
      applyInverse(box);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void ActionFilterBar::setStates(const QHash<QCheckBox*, bool>& wanted) {
  bool actionChanged = false;
  bool unchangedChanged = false;
  bool excludedChanged = false;
  for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
    QCheckBox* box = it.key();
    const bool value = it.value();
    if (box == nullptr || box->isChecked() == value) continue;
    box->blockSignals(true);
    box->setChecked(value);
    box->blockSignals(false);
    if (box == excluded_) {
      excludedChanged = true;
    } else if (box == unchanged_) {
      unchangedChanged = true;
    } else {
      actionChanged = true;
    }
  }
  if (actionChanged || excludedChanged) emit filterChanged();
  if (unchangedChanged) emit unchangedToggled(unchanged_->isChecked());
}

void ActionFilterBar::resetDefaults() {
  QHash<QCheckBox*, bool> wanted;
  for (QCheckBox* box : actions_) wanted.insert(box, true);
  wanted.insert(unchanged_, false);
  wanted.insert(excluded_, false);
  setStates(wanted);
  // The folder toggle is one of the decision filters and returns to off.
  directories_->setChecked(false);
}

void ActionFilterBar::applyInverse(QCheckBox* box) {
  // The press/release filter and the clicked() fallback can both observe
  // the same physical click. Reset only on a new press, not by elapsed time:
  // a slow click can exceed any timeout and get inverted twice.
  if (inverseHandled_) return;
  inverseHandled_ = true;
  const QVector<QCheckBox*> boxes = allBoxes();
  if (box == excluded_) {
    bool onlyExcluded = box->isChecked();
    for (QCheckBox* other : boxes) {
      if (other != box && other->isChecked()) onlyExcluded = false;
    }
    if (onlyExcluded) {
      resetDefaults();
    } else {
      QHash<QCheckBox*, bool> wanted;
      for (QCheckBox* action : actions_) wanted.insert(action, false);
      wanted.insert(unchanged_, false);
      wanted.insert(excluded_, true);
      setStates(wanted);
    }
    return;
  }
  if (!box->isChecked() && box == unchanged_) {
    bool allActions = true;
    for (QCheckBox* action : actions_) {
      if (!action->isChecked()) allActions = false;
    }
    if (allActions) {
      QHash<QCheckBox*, bool> wanted;
      wanted.insert(box, true);
      setStates(wanted);
      return;
    }
  }
  if (box->isChecked()) {
    bool anyOther = false;
    for (QCheckBox* other : boxes) {
      if (other != box && other->isChecked()) anyOther = true;
    }
    if (anyOther) {
      QHash<QCheckBox*, bool> wanted;
      for (QCheckBox* other : boxes) wanted.insert(other, other == box);
      setStates(wanted);
      return;
    }
  }
  QHash<QCheckBox*, bool> wanted;
  for (QCheckBox* action : actions_) wanted.insert(action, true);
  wanted.insert(unchanged_, unchanged_->isChecked());
  wanted.insert(excluded_, excluded_->isChecked());
  setStates(wanted);
}

void ActionFilterBar::showBoxMenu(QCheckBox* box, const QPoint& point) {
  QMenu menu(this);
  QAction* only = menu.addAction(QStringLiteral("Show only this"));
  QAction* others =
      menu.addAction(QStringLiteral("Show everything but this"));
  QAction* every = menu.addAction(QStringLiteral("Show all"));
  menu.addSeparator();
  QAction* reset = menu.addAction(QStringLiteral("Reset to default"));
  QAction* chosen = menu.exec(box->mapToGlobal(point));
  const QVector<QCheckBox*> boxes = allBoxes();
  if (chosen == only) {
    QHash<QCheckBox*, bool> wanted;
    for (QCheckBox* other : boxes) wanted.insert(other, other == box);
    setStates(wanted);
  } else if (chosen == others) {
    QHash<QCheckBox*, bool> wanted;
    for (QCheckBox* other : boxes) wanted.insert(other, other != box);
    setStates(wanted);
  } else if (chosen == every) {
    QHash<QCheckBox*, bool> wanted;
    for (QCheckBox* other : boxes) wanted.insert(other, true);
    setStates(wanted);
  } else if (chosen == reset) {
    resetDefaults();
  }
}

void ActionFilterBar::showExcludedMenu(const QPoint& point) {
  QMenu menu(this);
  bool onlyExcluded = excluded_->isChecked();
  for (QCheckBox* action : actions_) {
    if (action->isChecked()) onlyExcluded = false;
  }
  if (unchanged_ != nullptr && unchanged_->isChecked()) onlyExcluded = false;
  QAction* invert = menu.addAction(onlyExcluded
                                       ? QStringLiteral("Show all decisions")
                                       : QStringLiteral("Show only excluded items"));
  QAction* reset = menu.addAction(
      excludedCount_ > 0
          ? QStringLiteral("Reset All Exclusions (%1)").arg(excludedCount_)
          : QStringLiteral("Reset All Exclusions"));
  reset->setEnabled(excludedCount_ > 0);
  QAction* chosen = menu.exec(excluded_->mapToGlobal(point));
  if (chosen == invert) {
    applyInverse(excluded_);
  } else if (chosen == reset && excludedCount_ > 0) {
    emit resetExclusionsRequested();
  }
}

bool ActionFilterBar::showExcluded() const { return excluded_->isChecked(); }
bool ActionFilterBar::filterDirectories() const {
  return directories_->isChecked();
}

void ActionFilterBar::setFilterDirectories(bool enabled) {
  directories_->setChecked(enabled);
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
  excludedCount_ = count;
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

void ActionFilterBar::toggleUnchanged() { unchanged_->click(); }

bool ActionFilterBar::showUnchanged() const { return unchanged_->isChecked(); }

void ActionFilterBar::setConflictsAvailable(bool available) {
  conflicts_->setVisible(available);
}

}  // namespace qt_rsync::ui
