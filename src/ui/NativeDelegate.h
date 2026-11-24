#pragma once

#include <QStyledItemDelegate>

#include <functional>

#include "model/DataStore.h"

namespace qt_rsync::ui {

// Ported from native-hybrid/hybrid.cpp: one delegate handles the folder tree,
// stripes, action colours and the decision checkbox/action-icon group.
class NativeDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  using DecisionCallback = std::function<void(const QString&, int)>;
  using CollapseCallback = std::function<void(int, bool)>;

  NativeDelegate(model::DataStore* store, int pane, QObject* parent = nullptr);

  void setDecisionCallback(DecisionCallback callback);
  void setCollapseCallback(CollapseCallback callback);

  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;
  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

 private:
  void paintCheck(QPainter* painter, const QStyleOptionViewItem& option,
                  const QModelIndex& index, int id) const;
  void paintBackground(QPainter* painter, const QStyleOptionViewItem& option,
                       bool striped) const;
  QColor textColor(const QStyleOptionViewItem& option, int id) const;
  void drawFolderIcon(QPainter* painter, const QRect& rect) const;
  void drawFileIcon(QPainter* painter, const QRect& rect,
                    const QColor& tint) const;

  model::DataStore* store_;
  int pane_;
  DecisionCallback decisionCallback_;
  CollapseCallback collapseCallback_;
};

}  // namespace qt_rsync::ui
