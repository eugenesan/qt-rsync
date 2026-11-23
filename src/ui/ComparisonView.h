#pragma once

#include <QVector>
#include <QWidget>

#include "core/Change.h"
#include "core/Decision.h"
#include "model/DataStore.h"
#include "ui/DecisionDelegate.h"

class QTableView;

namespace qt_rsync::ui {

// Three synchronized panes over one lifted DataStore. Sort/filter/decoration
// live in the C++ models; there is no parallel decision state here.
class ComparisonView : public QWidget {
  Q_OBJECT
 public:
  explicit ComparisonView(QWidget* parent = nullptr);

  [[nodiscard]] model::DataStore& store() { return store_; }
  [[nodiscard]] core::Decisions& decisions() { return decisions_; }
  void setChanges(const QVector<core::Change>& changes);

 signals:
  void decisionChanged();

 private slots:
  void applyDecision(const QString& path, int decision);

 private:
  void syncDecisionsToStore();

  model::DataStore store_;
  core::Decisions decisions_;
  QVector<QTableView*> views_;
  QVector<model::TableModel*> models_;
  DecisionDelegate* delegate_ = nullptr;
  DecisionKeyFilter* keyFilter_ = nullptr;
};

}  // namespace qt_rsync::ui
