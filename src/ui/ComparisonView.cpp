#include "ui/ComparisonView.h"

#include <QTableView>

namespace qt_rsync::ui {

ComparisonView::ComparisonView(QWidget* parent) : QSplitter(parent) {
  for (int i = 0; i < 3; ++i) addWidget(new QTableView(this));
  setChildrenCollapsible(false);
}

}  // namespace qt_rsync::ui
