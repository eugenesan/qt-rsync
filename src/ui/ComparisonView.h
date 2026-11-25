#pragma once

#include <QVector>

#include <vector>
#include <QWidget>

#include "core/Change.h"
#include "core/Decision.h"
#include "model/DataStore.h"
#include "ui/ActionFilterBar.h"
#include "ui/DecisionDelegate.h"
#include "ui/NativeDelegate.h"

class QTableView;

namespace qt_rsync::ui {

// Three synchronized panes over one lifted DataStore. Sort/filter/decoration
// live in the C++ models; there is no parallel decision state here.
class ComparisonView : public QWidget {
  Q_OBJECT
 public:
  explicit ComparisonView(QWidget* parent = nullptr);

  [[nodiscard]] model::DataStore& store() { return store_; }
  [[nodiscard]] ActionFilterBar* filterBar() { return filterBar_; }
  void applyFilters();
  void setFilterBar(ActionFilterBar* bar);
  [[nodiscard]] core::Decisions& decisions() { return decisions_; }
  void setChanges(const QVector<core::Change>& changes);
  void setShowUnchanged(bool show);
  void setActionColours(const QVector<QString>& colours);
  void setRootNames(const QString& source, const QString& destination);
  void selectPath(const QString& path);
  // Reveal a folder from the overview: expand its ancestors, keep the row
  // centered in all panes and move the keyboard focus to the list.
  void activatePath(const QString& path);
  void jumpToEdge(int pane, bool last);
  void moveHierarchy(int pane, int direction);
  bool handleCollapseKey(int pane, int key);
  [[nodiscard]] QStringList searchMatches(const QString& text) const;
  void foldCurrent(int pane, bool collapse);
  void foldAll(bool collapse);
  void applyDestinationMetadata(
      const QHash<QString, QPair<qlonglong, QString>>& metadata);
  void markDelete(const QString& path);
  void resetDecision(const QString& path);
  void resetSubtreeDecisions(const QString& path);
  void resetAllDecisions();
  void resetExclusions();
  void applyDecision(const QString& path, int decision);
  bool undoDecision();
  bool redoDecision();

 signals:
  void decisionChanged();
  void viewChanged();
  void undoAvailable(bool available);
  void redoAvailable(bool available);
  void batchStarted();
  void batchFinished();
  void filterRequested(const QString& pattern, const QString& path);
  void deleteFilterRequested(const QString& pattern, const QString& path);
  void deleteRequested(const QString& path);
  void diffRequested(const QString& path);
  void openDirectoryRequested(int pane, const QString& path);

 private slots:
  void startDecisionBatch();
  void endDecisionBatch();

 private:
  void syncDecisionsToStore();
  void onSortIndicatorChanged(int pane, int section, Qt::SortOrder order);
  void onCurrentChanged(int origin, const QModelIndex& index);
  void showContextMenu(int pane, const QPoint& position);
  void showHeaderMenu(int pane, const QPoint& position);

  void selectRow(int row);
  void restoreFocusAfterDecision(const std::vector<int>& oldVisible,
                                 const QStringList& affectedPaths);

  bool batching_ = false;
  std::vector<int> batchOldVisible_;
  QStringList batchAffectedPaths_;
  QHash<QString, core::Decision> batchBefore_;
  bool sorting_ = false;
  bool selecting_ = false;

  model::DataStore store_;
  ActionFilterBar* filterBar_ = nullptr;
  core::Decisions decisions_;
  QVector<QTableView*> views_;
  QVector<model::TableModel*> models_;
  QVector<NativeDelegate*> delegates_;
  DecisionKeyFilter* keyFilter_ = nullptr;
  QVector<QHash<QString, core::Decision>> decisionHistory_;
  QVector<QHash<QString, core::Decision>> redoHistory_;
};

}  // namespace qt_rsync::ui
