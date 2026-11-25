#include "ui/NativeDelegate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionViewItem>

#include "ui/Icons.h"

namespace qt_rsync::ui {

using namespace model;

NativeDelegate::NativeDelegate(DataStore* store, int pane, QObject* parent)
    : QStyledItemDelegate(parent), store_(store), pane_(pane) {}

void NativeDelegate::setDecisionCallback(DecisionCallback callback) {
  decisionCallback_ = std::move(callback);
}

void NativeDelegate::setCollapseCallback(CollapseCallback callback) {
  collapseCallback_ = std::move(callback);
}

QSize NativeDelegate::sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex&) const {
  QSize size = QStyledItemDelegate::sizeHint(option, QModelIndex());
  size.setHeight(qMax(22, size.height()));
  return size;
}

bool NativeDelegate::editorEvent(QEvent* event, QAbstractItemModel*,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) {
  if (event->type() != QEvent::MouseButtonPress) return false;
  auto* mouse = static_cast<QMouseEvent*>(event);
  if (mouse->button() != Qt::LeftButton) return false;
  const int row = index.row();
  if (row < 0 || row >= static_cast<int>(store_->visible.size())) return false;
  const int id = store_->visible[row];

  if (pane_ == 1 && index.column() == 4) {
    const int start = option.rect.left() +
                      qMax(0, (option.rect.width() - (16 + 8 + 16)) / 2);
    const QRect indicator(start, option.rect.center().y() - 8, 16, 16);
    if (!indicator.contains(mouse->pos())) return true;
    const QString path = QString::fromStdString(store_->paths[id]);
    const int decision = store_->decisions[id] == Exclude ? Inherit : Exclude;
    if (decisionCallback_) decisionCallback_(path, decision);
    return true;
  }

  if (id >= static_cast<int>(store_->is_dir.size()) || !store_->is_dir[id]) {
    return false;
  }
  const int depth = id < static_cast<int>(store_->depth.size()) ? store_->depth[id] : 0;
  const QRect hit(option.rect.left() + 4 + depth * 14 - 1,
                  option.rect.center().y() - 5, 10, 10);
  if (!hit.contains(mouse->pos())) return false;
  if (collapseCallback_) collapseCallback_(id, !store_->is_collapsed(id));
  return true;
}

void NativeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const {
  const int row = index.row();
  const int col = index.column();
  if (row < 0 || row >= static_cast<int>(store_->visible.size())) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }
  const int id = store_->visible[row];
  if (pane_ == 1) {
    paintCheck(painter, option, index, id);
    return;
  }

  const bool dirRow = id < static_cast<int>(store_->is_dir.size()) && store_->is_dir[id];
  // An item is only drawn on the side where it exists. A new directory has no
  // destination row (no chevron, no folder icon, no name), and a deleted or
  // destination-only directory has no source row.
  if (!store_->present_on_pane(id, pane_)) {
    QStyleOptionViewItem blank(option);
    initStyleOption(&blank, index);
    paintBackground(painter, option, false);
    return;
  }

  const bool nameColumn = col == 0 || col == 5;
  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  const bool selected = opt.state & QStyle::State_Selected;
  const bool striped = !selected && row < static_cast<int>(store_->stripe.size()) &&
                       store_->stripe[row] != 0;
  paintBackground(painter, option, striped);

  QColor colour = textColor(opt, id);
  if (!colour.isValid()) colour = opt.palette.color(QPalette::Text);

  if (!nameColumn) {
    painter->save();
    painter->setPen(colour);
    painter->drawText(opt.rect.adjusted(2, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
    return;
  }

  const bool isDir = dirRow;
  const int depth = id < static_cast<int>(store_->depth.size()) ? store_->depth[id] : 0;
  int x = opt.rect.left() + 4 + depth * 14;
  const int mid = opt.rect.center().y();
  const int baseline = opt.rect.top() + opt.rect.height() / 2 +
                       opt.fontMetrics.ascent() / 2 - 1;
  painter->save();

  const bool rootRow = store_->paths[id] == DataStore::root_path();
  if (rootRow) {
    if (!store_->root_icon.isNull()) {
      store_->root_icon.paint(painter, QRect(x, mid - 8, 16, 16));
    } else {
      drawFolderIcon(painter, QRect(x, mid - 8, 16, 14));
    }
    x += 20;
  } else if (isDir) {
    const bool expanded = !store_->is_collapsed(id);
    const bool hasChildren = id < static_cast<int>(store_->dir_has_children.size()) &&
                             store_->dir_has_children[id];
    if (hasChildren) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(colour);
      QRect chevron(x, mid - 4, 8, 8);
      QPoint triangle[3];
      if (expanded) {
        triangle[0] = QPoint(chevron.left(), chevron.top());
        triangle[1] = QPoint(chevron.right(), chevron.top());
        triangle[2] = QPoint(chevron.center().x(), chevron.bottom());
      } else {
        triangle[0] = QPoint(chevron.left(), chevron.top());
        triangle[1] = QPoint(chevron.right(), chevron.center().y());
        triangle[2] = QPoint(chevron.left(), chevron.bottom());
      }
      painter->drawPolygon(triangle, 3);
    }
    const int iconX = x + 14;
    if (!store_->folder_icon.isNull()) {
      store_->folder_icon.paint(painter, QRect(iconX, mid - 8, 16, 16));
    } else {
      drawFolderIcon(painter, QRect(iconX, mid - 8, 16, 14));
    }
    x = iconX + 20;
  } else {
    const int iconX = x + 14;
    painter->setPen(QPen(QColor("#D0D7DE"), 1));
    painter->drawLine(x + 10, opt.rect.top(), x + 10, opt.rect.bottom());
    const std::string path = store_->paths[id];
    const std::size_t dot = path.rfind('.');
    const std::size_t slash = path.rfind('/');
    std::string suffix;
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
      suffix = path.substr(dot + 1);
    }
    auto found = store_->file_icons.find(suffix);
    if (found == store_->file_icons.end()) found = store_->file_icons.find("");
    if (found != store_->file_icons.end() && !found->second.isNull()) {
      found->second.paint(painter, QRect(iconX, mid - 8, 16, 16));
    } else {
      drawFileIcon(painter, QRect(iconX + 1, mid - 9, 14, 16), colour);
    }
    x = iconX + 20;
  }

  painter->setPen(colour);
  const int available = opt.rect.right() - x - 4;
  painter->drawText(x, baseline,
                    opt.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                               Qt::ElideRight, available));
  painter->restore();
}

void NativeDelegate::paintCheck(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index, int id) const {
  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  paintBackground(painter, option, false);
  const bool deleteRequest = store_->decisions[id] == DeleteRequest;
  const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
  const int groupWidth = deleteRequest ? 16 : 16 + 8 + 16;
  const int start = opt.rect.left() + qMax(0, (opt.rect.width() - groupWidth) / 2);
  QRect box(start, opt.rect.center().y() - 8, 16, 16);
  if (!deleteRequest) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (checked) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(opt.palette.color(QPalette::Highlight));
      painter->drawRoundedRect(box, 4, 4);
      painter->setPen(QPen(opt.palette.color(QPalette::HighlightedText), 2,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter->drawLine(box.left() + 4, box.center().y(),
                        box.left() + 7, box.bottom() - 4);
      painter->drawLine(box.left() + 7, box.bottom() - 4,
                        box.right() - 4, box.top() + 4);
    } else {
      // An unchecked decision is an empty outlined box. Filling it with the
      // highlight colour made on and off states indistinguishable.
      QColor outline = opt.palette.color(QPalette::Mid);
      if (!outline.isValid()) outline = opt.palette.color(QPalette::Text);
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(outline, 1.4));
      painter->drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
    painter->restore();
  }
  int action = id < static_cast<int>(store_->actions.size()) ? store_->actions[id] : NoChange;
  if (deleteRequest) action = Delete;
  const QIcon icon = icons::actionIcon(action);
  if (!icon.isNull()) {
    icon.paint(painter, QRect(deleteRequest ? start : start + 24,
                              opt.rect.center().y() - 8, 16, 16));
  }
}

void NativeDelegate::paintBackground(QPainter* painter,
                                     const QStyleOptionViewItem& option,
                                     bool striped) const {
  QStyleOptionViewItem blank(option);
  blank.text.clear();
  blank.icon = QIcon();
  blank.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
  if (striped) painter->fillRect(blank.rect, QColor(0, 0, 0, 14));
  const QWidget* widget = blank.widget;
  QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, &blank, painter, widget);
}

QColor NativeDelegate::textColor(const QStyleOptionViewItem& option, int id) const {
  if (option.state & QStyle::State_Selected) return {};
  if (id >= 0 && id < static_cast<int>(store_->faded.size()) &&
      store_->faded[id]) {
    return QColor(QStringLiteral("#9CA3AF"));
  }
  const int action = id < static_cast<int>(store_->actions.size())
                         ? store_->actions[id] : NoChange;
  if (action >= 0 && action < 6) {
    static const QVector<QString> defaults{
        QStringLiteral("#208020"), QStringLiteral("#eab308"),
        QStringLiteral("#7c3aed"), QStringLiteral("#ef4444"),
        QStringLiteral("#a05020"), QStringLiteral("#94a3b8")};
    if (action < static_cast<int>(store_->action_colours.size())) {
      const QString& chosen = store_->action_colours.at(action);
      // An empty entry means the user switched the marking off.
      return chosen.isEmpty() ? QColor() : QColor(chosen);
    }
    return QColor(defaults.at(action));
  }
  if (store_->decisions[id] == Exclude) return QColor("#9CA3AF");
  return {};
}

void NativeDelegate::drawFolderIcon(QPainter* painter, const QRect& rect) const {
  painter->save();
  painter->setPen(QPen(QColor("#B8860B"), 1));
  painter->setBrush(QColor("#F4C542"));
  painter->drawRect(QRect(rect.left(), rect.top() + 3, rect.width(), rect.height() - 3));
  painter->drawRect(QRect(rect.left(), rect.top(), rect.width() / 2, 4));
  painter->restore();
}

void NativeDelegate::drawFileIcon(QPainter* painter, const QRect& rect,
                                  const QColor& tint) const {
  painter->save();
  QColor fill = tint.isValid() ? tint : QColor("#EAB308");
  fill.setAlpha(48);
  painter->setBrush(fill);
  painter->setPen(QPen(tint.isValid() ? tint : QColor("#B8860B"), 1));
  painter->drawRect(rect);
  painter->drawLine(rect.right() - 4, rect.top(), rect.right() - 4, rect.top() + 4);
  painter->drawLine(rect.right() - 4, rect.top() + 4, rect.right(), rect.top() + 4);
  painter->restore();
}

}  // namespace qt_rsync::ui
