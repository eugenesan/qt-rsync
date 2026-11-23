#include "ui/OverviewPane.h"

#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>

namespace qt_rsync::ui {

OverviewPane::OverviewPane(QWidget* parent)
    : QWidget(parent), tree_(new QTreeView(this)), model_(new model::OverviewModel(this)) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  tree_->setModel(model_);
  tree_->header()->setStretchLastSection(true);
  layout->addWidget(tree_);
}

void OverviewPane::rebuild(const model::DataStore& store) {
  model_->build(store, false);
  tree_->expandAll();
}

}  // namespace qt_rsync::ui
