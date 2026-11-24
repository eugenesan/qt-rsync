#include "ui/Icons.h"

#include <QColor>
#include <QApplication>
#include <QIcon>
#include <QStyle>
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
  auto themed = [](std::initializer_list<const char*> names,
                   QStyle::StandardPixmap fallback) {
    for (const char* candidate : names) {
      const QIcon icon = QIcon::fromTheme(QString::fromLatin1(candidate));
      if (!icon.isNull()) return icon;
    }
    return QApplication::style()->standardIcon(fallback);
  };

  if (name == QLatin1String("swap")) {
    return themed({"swap-panels-symbolic", "swap-panels"}, QStyle::SP_BrowserReload);
  }
  if (name == QLatin1String("compare")) {
    return themed({"kr_comparedirs", "kr_comparedir", "document-compareright",
                   "document-compareright-symbolic"},
                  QStyle::SP_FileDialogContentsView);
  }
  if (name == QLatin1String("verify")) {
    return themed({"answer-symbolic", "answer", "answer-correct", "Answer"},
                  QStyle::SP_DialogApplyButton);
  }
  if (name == QLatin1String("sync")) {
    return themed({"media-playback-start-symbolic", "media-playback-start"},
                  QStyle::SP_MediaPlay);
  }
  if (name == QLatin1String("cancel")) {
    return themed({"process-stop", "dialog-cancel"}, QStyle::SP_DialogCancelButton);
  }
  if (name == QLatin1String("pause")) {
    return themed({"media-playback-pause"}, QStyle::SP_MediaPause);
  }
  if (name == QLatin1String("undo")) {
    return themed({"anchor-symbolic", "anchor"}, QStyle::SP_BrowserReload);
  }
  if (name == QLatin1String("search")) {
    return themed({"edit-find"}, QStyle::SP_FileDialogContentsView);
  }
  if (name == QLatin1String("settings")) {
    const QIcon gear = QIcon::fromTheme(QStringLiteral("settings-configure-symbolic"));
    if (!gear.isNull()) return gear;
    return themed({"configure"}, QStyle::SP_FileDialogDetailedView);
  }
  if (name == QLatin1String("folder")) {
    return themed({"inode-directory-symbolic", "inode-directory",
                   "folder-open-symbolic", "folder-open", "folder"},
                  QStyle::SP_DirOpenIcon);
  }
  if (name == QLatin1String("filters")) {
    return themed({"view-filter", "view-list-details"},
                  QStyle::SP_FileDialogDetailedView);
  }
  if (name == QLatin1String("inventory")) {
    return themed({"view-list-details", "view-list-tree"},
                  QStyle::SP_FileDialogListView);
  }
  if (name == QLatin1String("backup")) {
    return themed({"backup-symbolic", "folder-backup-symbolic",
                   "document-save-symbolic", "drive-harddisk"},
                  QStyle::SP_DriveHDIcon);
  }
  if (name == QLatin1String("log")) {
    return themed({"folder-log-symbolic", "folder-log", "text-x-generic-symbolic",
                   "text-x-generic", "text-plain"}, QStyle::SP_FileIcon);
  }
  if (name == QLatin1String("open")) {
    return themed({"document-open", "folder-open"}, QStyle::SP_DialogOpenButton);
  }
  return QIcon();
}

}  // namespace qt_rsync::ui::icons
