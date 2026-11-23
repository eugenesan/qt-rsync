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

  delegate_ = new DecisionDelegate(this);
  views_[1]->setItemDelegateForColumn(4, delegate_);
  keyFilter_ = new DecisionKeyFilter(this);
  connect(delegate_, &DecisionDelegate::decisionToggled,
          this, &ComparisonView::applyDecision);
  connect(keyFilter_, &DecisionKeyFilter::decisionToggled,
          this, &ComparisonView::applyDecision);
  for (QTableView* view : views_) {
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->installEventFilter(keyFilter_);
  }
  views_[0]->horizontalHeader()->setStretchLastSection(true);
  views_[2]->horizontalHeader()->setStretchLastSection(true);

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (i == j) continue;
      connect(views_[i]->verticalScrollBar(), &QScrollBar::valueChanged,
              views_[j]->verticalScrollBar(), &QScrollBar::setValue);
    }
    connect(views_[i]->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this, i](const QModelIndex& index) { onCurrentChanged(i, index); });
    connect(views_[i]->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this, i](int section, Qt::SortOrder order) {
              onSortIndicatorChanged(i, section, order);
            });
  }
}

void ComparisonView::setChanges(const QVector<core::Change>& changes) {
  decisions_.reconcile(changes, true);
  loadChanges(store_, changes);
  syncDecisionsToStore();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
}

void ComparisonView::applyDecision(const QString& path, int decision) {
  const core::Decision value = static_cast<core::Decision>(decision);
  decisions_.set(path, value);
  syncDecisionsToStore();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
}

void ComparisonView::syncDecisionsToStore() {
  const auto items = decisions_.explicitItems();
  for (const auto& item : items) {
    const std::string key = item.first.toStdString();
    for (std::size_t id = 0; id < store_.paths.size(); ++id) {
      const std::string& path = store_.paths[id];
      if (path != key && path.rfind(key + '/', 0) != 0) continue;
      if (id < store_.decisions.size()) {
        store_.decisions[id] = static_cast<int>(item.second);
      }
    }
  }
}

void ComparisonView::onCurrentChanged(int origin, const QModelIndex& index) {
  if (selecting_ || !index.isValid()) return;
  selecting_ = true;
  for (int pane = 0; pane < 3; ++pane) {
    if (pane == origin) continue;
    const QModelIndex target = views_[pane]->model()->index(index.row(), 0);
    views_[pane]->setCurrentIndex(target);
  }
  selecting_ = false;
}

void ComparisonView::onSortIndicatorChanged(int pane, int section,
                                            Qt::SortOrder order) {
  if (sorting_) return;
  sorting_ = true;
  const int logical = (pane == 1) ? 4 : section;
  models_[pane]->sort(logical, order);
  for (int other = 0; other < 3; ++other) {
    QHeaderView* header = views_[other]->horizontalHeader();
    const bool blocked = header->blockSignals(true);
    header->setSortIndicator(other == 1 ? 4 : section, order);
    header->blockSignals(blocked);
  }
  sorting_ = false;
}

}  // namespace qt_rsync::ui
