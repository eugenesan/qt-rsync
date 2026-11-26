#pragma once

#include <QSet>
#include <QString>
#include <QWidget>

#include "model/DataStore.h"
#include "ui/StatsBar.h"

class QTreeView;

namespace qt_rsync::ui {

class OverviewPane : public QWidget {
  Q_OBJECT
 public:
  explicit OverviewPane(QWidget* parent = nullptr);

  void rebuild(const model::DataStore& store);
  void refreshStats(const model::DataStore& store);
  void setActionColours(const QVector<QString>& colours);

 signals:
  void folderActivated(const QString& path);
  void excludeRequested(const QString& path);
  void restoreRequested(const QString& path);
  void filterRequested(const QString& pattern, const QString& path);

 private:
  void showFolderMenu(const QString& path, const QPoint& globalPosition);
  void updateStats(const model::DataStore& store);

  QTreeView* tree_;
  model::OverviewModel* model_;
  StatsBar* stats_;
  QSet<QString> expandedPaths_;
  bool firstBuild_ = true;
};

}  // namespace qt_rsync::ui
