#include "ui/TreeDelegate.h"

#include <QApplication>
#include <QPainter>

namespace qt_rsync::ui {

TreeDelegate::TreeDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize TreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                             const QModelIndex&) const {
  QSize size = QStyledItemDelegate::sizeHint(option, QModelIndex());
  size.setHeight(qMax(20, size.height()));
  return size;
}

void TreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const {
  const bool isName = index.column() == 0 || index.column() == 5;
  if (!isName) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  QStyleOptionViewItem styled(option);
  initStyleOption(&styled, index);
  const QWidget* widget = styled.widget;
  QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, &styled, painter, widget);

  const int depth = index.data(Qt::UserRole + 21).toInt();
  const bool collapsed = index.data(Qt::UserRole + 22).toBool();
  QRect rect = option.rect.adjusted(4 + depth * 14, 0, -2, 0);
  const bool directory = index.data(Qt::UserRole + 23).toBool();
  if (directory) {
    painter->save();
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                      collapsed ? QStringLiteral("\u25B8") : QStringLiteral("\u25BE"));
    painter->restore();
    rect.adjust(14, 0, 0, 0);
  }

  const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
  if (!icon.isNull()) {
    icon.paint(painter, QRect(rect.left(), rect.center().y() - 8, 16, 16));
    rect.adjust(20, 0, 0, 0);
  }

  painter->save();
  painter->setPen(styled.palette.color(option.state & QStyle::State_Selected
                                           ? QPalette::HighlightedText
                                           : QPalette::Text));
  painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                    index.data(Qt::DisplayRole).toString());
  painter->restore();
}

}  // namespace qt_rsync::ui
