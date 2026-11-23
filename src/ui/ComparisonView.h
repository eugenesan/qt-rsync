#pragma once

#include <QSplitter>

namespace qt_rsync::ui {

// Placeholder for the three-pane source / decision / destination view and
// its QAbstractTableModel-based delegates (lifted from native-hybrid).
class ComparisonView : public QSplitter {
  Q_OBJECT
 public:
  explicit ComparisonView(QWidget* parent = nullptr);
};

}  // namespace qt_rsync::ui
