#include "ui/Icons.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

#include <functional>
#include <QHash>
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

namespace {

QIcon makeGrid(std::function<void(QPainter&)> draw) {
  QPixmap pixmap(64, 64);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.scale(4.0, 4.0);
  painter.setRenderHint(QPainter::Antialiasing, true);
  draw(painter);
  painter.end();
  return QIcon(pixmap);
}

QPen gridPen(const QColor& colour, double width = 1.6) {
  QPen pen(colour, width);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  return pen;
}

void arrowHead(QPainter& p, const QPointF& tip, const QString& direction,
               const QColor& colour) {
  p.setPen(gridPen(colour));
  p.setBrush(QBrush(colour));
  QPointF a, b, c = tip;
  if (direction == QLatin1String("left")) {
    a = tip + QPointF(4, -3);
    b = tip + QPointF(4, 3);
  } else if (direction == QLatin1String("right")) {
    a = tip + QPointF(-4, -3);
    b = tip + QPointF(-4, 3);
  } else if (direction == QLatin1String("up")) {
    a = tip + QPointF(-3, 4);
    b = tip + QPointF(3, 4);
  } else {
    a = tip + QPointF(-3, -4);
    b = tip + QPointF(3, -4);
  }
  QPainterPath path(c);
  path.lineTo(a);
  path.lineTo(b);
  path.closeSubpath();
  p.drawPath(path);
}

}  // namespace

QIcon actionIcon(int action) {
  const QVector<QColor> colours{
      QColor("#208020"), QColor("#eab308"), QColor("#7c3aed"),
      QColor("#ef4444"), QColor("#a05020"), QColor("#64748b"),
      QColor("#64748b")};
  const QColor colour = colours.value(action, QColor("#64748b"));
  switch (action) {
    case 0:  // create
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.drawLine(QPointF(8, 3.5), QPointF(8, 12.5));
        p.drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
      });
    case 1:  // update
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.drawLine(QPointF(8, 12.5), QPointF(8, 4.5));
        arrowHead(p, QPointF(8, 3.0), QStringLiteral("up"), colour);
      });
    case 2:  // metadata
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.setBrush(QBrush(colour));
        p.drawEllipse(QRectF(6.9, 3.0, 2.2, 2.2));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(8, 6.6), QPointF(8, 13.0));
      });
    case 3:  // delete
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.drawLine(QPointF(4, 4), QPointF(12, 12));
        p.drawLine(QPointF(12, 4), QPointF(4, 12));
      });
    case 4:  // destination only
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.drawLine(QPointF(12, 8), QPointF(4, 8));
        arrowHead(p, QPointF(3, 8), QStringLiteral("left"), colour);
      });
    default:  // unchanged
      return makeGrid([colour](QPainter& p) {
        p.setPen(gridPen(colour));
        p.drawLine(QPointF(4, 6), QPointF(12, 6));
        p.drawLine(QPointF(4, 10), QPointF(12, 10));
      });
  }
}

QIcon fileIcon(const QString&) {
  return actionIcon(5);  // neutral glyph; file-type icons are not generated yet
}

QIcon folderIcon() {
  return makeGrid([](QPainter& p) {
    p.setPen(gridPen(QColor("#334155")));
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(2.5, 13);
    path.lineTo(2.5, 4);
    path.lineTo(6.5, 4);
    path.lineTo(8, 6);
    path.lineTo(13.5, 6);
    path.lineTo(13.5, 13);
    path.closeSubpath();
    p.drawPath(path);
  });
}

QIcon rootFolderIcon() {
  return folderIcon();
}

QIcon toolbarIcon(const QString& name) {
  if (name == QLatin1String("compare")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#0891B2"), 1.6, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(3, 8, 12, 8);
      p.drawLine(9, 4, 13, 8);
      p.drawLine(9, 12, 13, 8);
    });
  }
  if (name == QLatin1String("sync")) {
    return make([](QPainter& p) {
      p.setBrush(QColor("#15803D"));
      p.setPen(Qt::NoPen);
      QPolygonF triangle;
      triangle << QPointF(4, 3) << QPointF(13, 8) << QPointF(4, 13);
      p.drawPolygon(triangle);
    });
  }
  if (name == QLatin1String("verify")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#15803D"), 2, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(3, 8, 7, 12);
      p.drawLine(7, 12, 13, 4);
    });
  }
  if (name == QLatin1String("cancel")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#B91C1C"), 2, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(4, 4, 12, 12);
      p.drawLine(12, 4, 4, 12);
    });
  }
  if (name == QLatin1String("undo")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#6B7280"), 1.6, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(3, 8, 13, 8);
      p.drawLine(7, 4, 3, 8);
      p.drawLine(7, 12, 3, 8);
    });
  }
  if (name == QLatin1String("settings")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#6B7280"), 1.4));
      p.drawEllipse(QPointF(8, 8), 5, 5);
      p.drawEllipse(QPointF(8, 8), 2, 2);
    });
  }
  if (name == QLatin1String("search")) {
    return make([](QPainter& p) {
      p.setPen(QPen(QColor("#6B7280"), 1.6));
      p.drawEllipse(QPointF(6, 6), 4, 4);
      p.drawLine(9, 9, 13, 13);
    });
  }
  return QIcon();
}

}  // namespace qt_rsync::ui::icons
