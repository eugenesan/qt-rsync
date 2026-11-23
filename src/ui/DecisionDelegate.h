#pragma once

#include <QStyledItemDelegate>

class QTableView;

namespace qt_rsync::ui {

// Decision-column delegate and Space handler. They only emit path/decision
// pairs; the single core::Decisions store is updated by ComparisonView.
class DecisionDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  explicit DecisionDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 signals:
  void decisionToggled(const QString& path, int decision);
};

class DecisionKeyFilter : public QObject {
  Q_OBJECT
 public:
  explicit DecisionKeyFilter(QObject* parent = nullptr);
  bool eventFilter(QObject* watched, QEvent* event) override;

 signals:
  void decisionToggled(const QString& path, int decision);

 private:
  static int indexOfPath(QTableView* view, const QString& path);
};

}  // namespace qt_rsync::ui
