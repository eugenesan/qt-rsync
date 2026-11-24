#include "ui/StatsBar.h"

#include <QPainter>

#include "ui/Icons.h"

namespace qt_rsync::ui {

StatsBar::StatsBar(QWidget* parent) : QWidget(parent) {
  setFixedHeight(24);
  setToolTip(
      QStringLiteral(
          "Selected: the items that are checked in the comparison,\n"
          "with their total size and the count of each action. The icons "
          "below are the same actions."));
}

void StatsBar::setEntries(const QVector<QPair<QString, QString>>& entries) {
  entries_ = entries;
  updateGeometry();
  update();
}

void StatsBar::setActionColours(const QVector<QString>& colours) {
  actionIcons_.clear();
  for (int action = 0; action < 5; ++action) {
    if (action < colours.size() && !colours.at(action).isEmpty()) {
      actionIcons_.append(
          icons::actionIcon(action, QColor(colours.at(action))));
    } else {
      actionIcons_.append(icons::actionIcon(action));
    }
  }
  update();
}

void StatsBar::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  int x = 4;
  const int baseline = height() / 2 + fontMetrics().ascent() / 2 - 1;
  if (actionIcons_.isEmpty()) {
    for (int action = 0; action < 5; ++action) {
      actionIcons_.append(icons::actionIcon(action));
    }
  }
  for (int i = 0; i < entries_.size(); ++i) {
    const auto& entry = entries_.at(i);
    painter.save();
    if (i > 0 && i - 1 < actionIcons_.size()) {
      actionIcons_.at(i - 1).paint(&painter, QRect(x, (height() - 16) / 2, 16, 16));
      x += 20;
    }
    painter.setPen(QColor(entry.second));
    painter.drawText(x, baseline, entry.first);
    x += fontMetrics().horizontalAdvance(entry.first) + 14;
    painter.restore();
  }
}

}  // namespace qt_rsync::ui
