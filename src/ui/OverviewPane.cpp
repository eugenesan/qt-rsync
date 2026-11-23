#include "ui/OverviewPane.h"

#include <QHBoxLayout>
#include <QTreeView>

namespace qt_rsync::ui {

OverviewPane::OverviewPane(QWidget* parent)
    : QWidget(parent), tree_(new QTreeView(this)) {
  auto* layout = new QHBoxLayout(this);
  layout->addWidget(tree_);
}

}  // namespace qt_rsync::ui
