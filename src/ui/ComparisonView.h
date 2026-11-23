#pragma once

#include <QVector>
#include <QWidget>

#include "core/Change.h"
#include "model/DataStore.h"

class QTableView;

namespace qt_rsync::ui {

// Three synchronized panes over one lifted DataStore. Sort/filter/decoration
// live in the C++ models; there is no parallel decision state here.
class ComparisonView : public QWidget {
  Q_OBJECT
 public:
  explicit ComparisonView(QWidget* parent = nullptr);

  [[nodiscard]] model::DataStore& store() { return store_; }
  void setChanges(const QVector<core::Change>& changes);

 private:
  model::DataStore store_;
  QVector<QTableView*> views_;
  QVector<model::TableModel*> models_;
};

}  // namespace qt_rsync::ui
