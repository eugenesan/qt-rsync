#include "ui/StatsBar.h"

#include <QPainter>

#include "ui/Icons.h"

namespace qt_rsync::ui {

StatsBar::StatsBar(QWidget* parent) : QWidget(parent) {
  setFixedHeight(24);
}

void StatsBar::setEntries(const QVector<QPair<QString, QString>>& entries) {
  entries_ = entries;
  updateGeometry();
  update();
}

void StatsBar::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  int x = 4;
  const int baseline = height() / 2 + fontMetrics().ascent() / 2 - 1;
  static const QVector<QIcon> actionIcons{
      icons::actionIcon(0), icons::actionIcon(1), icons::actionIcon(2),
      icons::actionIcon(3), icons::actionIcon(4)};
  for (int i = 0; i < entries_.size(); ++i) {
    const auto& entry = entries_.at(i);
    painter.save();
    if (i > 0 && i - 1 < actionIcons.size()) {
      actionIcons.at(i - 1).paint(&painter, QRect(x, (height() - 16) / 2, 16, 16));
      x += 20;
    }
    painter.setPen(QColor(entry.second));
    painter.drawText(x, baseline, entry.first);
    x += fontMetrics().horizontalAdvance(entry.first) + 14;
    painter.restore();
  }
}

}  // namespace qt_rsync::ui
