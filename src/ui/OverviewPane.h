#pragma once

#include <QWidget>

class QTreeView;

namespace qt_rsync::ui {

// Placeholder for the folder overview and statistics bar.
class OverviewPane : public QWidget {
  Q_OBJECT
 public:
  explicit OverviewPane(QWidget* parent = nullptr);

 private:
  QTreeView* tree_;
};

}  // namespace qt_rsync::ui
