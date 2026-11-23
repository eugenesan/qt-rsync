#pragma once

#include <QWidget>

#include "model/DataStore.h"

class QTreeView;

namespace qt_rsync::ui {

class OverviewPane : public QWidget {
  Q_OBJECT
 public:
  explicit OverviewPane(QWidget* parent = nullptr);

  void rebuild(const model::DataStore& store);

 private:
  QTreeView* tree_;
  model::OverviewModel* model_;
};

}  // namespace qt_rsync::ui
