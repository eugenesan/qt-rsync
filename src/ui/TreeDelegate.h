#pragma once

#include <QStyledItemDelegate>

namespace qt_rsync::ui {

// Draws the folder chevron and indentation in the name columns, and keeps the
// row density close to the reference view.
class TreeDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  explicit TreeDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};

}  // namespace qt_rsync::ui
