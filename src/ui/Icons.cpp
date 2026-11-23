#include "ui/Icons.h"

#include <QColor>
#include <QPainter>

#include <functional>
#include <QPixmap>

namespace qt_rsync::ui::icons {
namespace {

QIcon make(std::function<void(QPainter&)> draw) {
  QPixmap pixmap(32, 32);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.scale(2.0, 2.0);
  draw(painter);
  painter.end();
  return QIcon(pixmap);
}

}  // namespace

QIcon actionIcon(int action) {
  switch (action) {
    case 0:  // create
      return make([](QPainter& p) {
        p.setPen(QPen(QColor("#16A34A"), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(8, 3, 8, 13);
        p.drawLine(3, 8, 13, 8);
      });
    case 1:  // update
      return make([](QPainter& p) {
        p.setPen(QPen(QColor("#EA580C"), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(8, 13, 8, 3);
        p.drawLine(4, 7, 8, 3);
        p.drawLine(12, 7, 8, 3);
      });
    case 2:  // metadata
      return make([](QPainter& p) {
        p.setBrush(QColor("#0891B2"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(8, 8), 3, 3);
      });
    case 3:  // delete
      return make([](QPainter& p) {
        p.setPen(QPen(QColor("#DC2626"), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(4, 4, 12, 12);
        p.drawLine(12, 4, 4, 12);
      });
    case 4:  // destination only
      return make([](QPainter& p) {
        p.setPen(QPen(QColor("#B45309"), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(13, 8, 3, 8);
        p.drawLine(7, 4, 3, 8);
        p.drawLine(7, 12, 3, 8);
      });
    default:
      return make([](QPainter& p) {
        p.setPen(QPen(QColor("#64748B"), 1.6));
        p.drawLine(4, 8, 12, 8);
      });
  }
}

QIcon fileIcon(const QString& name) {
  const QString lower = name.toLower();
  QColor colour("#6B7280");
  if (lower.endsWith(QLatin1String(".txt")) || lower.endsWith(QLatin1String(".md"))) {
    colour = QColor("#2563EB");
  } else if (lower.endsWith(QLatin1String(".json")) ||
             lower.endsWith(QLatin1String(".py"))) {
    colour = QColor("#D97706");
  }
  return make([colour](QPainter& p) {
    p.setPen(QPen(colour, 1.2));
    p.setBrush(QColor("#F8FAFC"));
    QPolygonF shape;
    shape << QPointF(3, 2) << QPointF(10, 2) << QPointF(13, 5)
          << QPointF(13, 14) << QPointF(3, 14);
    p.drawPolygon(shape);
    p.drawLine(10, 2, 10, 5);
    p.drawLine(10, 5, 13, 5);
  });
}

QIcon folderIcon() {
  return make([](QPainter& p) {
    p.setPen(QPen(QColor("#B8860B"), 1));
    p.setBrush(QColor("#F4C542"));
    p.drawRect(QRect(2, 5, 12, 8));
    p.drawRect(QRect(2, 3, 6, 3));
  });
}

QIcon rootFolderIcon() {
  return make([](QPainter& p) {
    p.setPen(QPen(QColor("#B8860B"), 1.2));
    p.setBrush(QColor("#F4C542"));
    p.drawRect(QRect(1, 4, 14, 10));
    p.drawRect(QRect(1, 2, 7, 3));
  });
}

}  // namespace qt_rsync::ui::icons
