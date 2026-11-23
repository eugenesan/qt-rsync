#include "ui/ComparisonView.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QTableView>

namespace qt_rsync::ui {

ComparisonView::ComparisonView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  for (int pane = 0; pane < 3; ++pane) {
    auto* view = new QTableView(this);
    view->verticalHeader()->hide();
    view->horizontalHeader()->setHighlightSections(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setAlternatingRowColors(false);
    auto* model = new model::TableModel(&store_, pane, this);
    view->setModel(model);
    views_.append(view);
    models_.append(model);
    layout->addWidget(view);
  }

  // Same visible columns as the proven hybrid view: source columns on the
  // left, the decision column in the middle, destination columns on the right.
  for (int column : {3, 4, 5, 6, 7, 8, 9, 10}) views_[0]->setColumnHidden(column, true);
  for (int column = 0; column < 11; ++column) {
    views_[1]->setColumnHidden(column, column != 4);
  }
  for (int column : {0, 1, 2, 3, 4, 8, 9, 10}) views_[2]->setColumnHidden(column, true);
  views_[1]->setFixedWidth(160);

  for (QTableView* view : views_) {
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  }
  views_[0]->horizontalHeader()->setStretchLastSection(true);
  views_[2]->horizontalHeader()->setStretchLastSection(true);

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (i == j) continue;
      connect(views_[i]->verticalScrollBar(), &QScrollBar::valueChanged,
              views_[j]->verticalScrollBar(), &QScrollBar::setValue);
    }
  }
}

void ComparisonView::setChanges(const QVector<core::Change>& changes) {
  loadChanges(store_, changes);
  for (model::TableModel* model : models_) model->reset();
}

}  // namespace qt_rsync::ui
