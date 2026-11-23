#include "ui/StatsBar.h"

#include <QPainter>

namespace qt_rsync::ui {

StatsBar::StatsBar(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(24);
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
  for (const auto& entry : entries_) {
    painter.save();
    painter.setPen(QColor(entry.second));
    painter.drawText(x, baseline, entry.first);
    x += fontMetrics().horizontalAdvance(entry.first) + 14;
    painter.restore();
  }
}

}  // namespace qt_rsync::ui
