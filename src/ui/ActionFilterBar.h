#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QToolButton;

namespace qt_rsync::ui {

// The checkbox row above the comparison lists. It filters the *view* only;
// it never changes the execution plan or the rsync command.
class ActionFilterBar : public QWidget {
  Q_OBJECT
 public:
  explicit ActionFilterBar(QWidget* parent = nullptr);

  [[nodiscard]] bool showExcluded() const;
  [[nodiscard]] bool filterDirectories() const;
  [[nodiscard]] bool actionVisible(int action) const;
  [[nodiscard]] bool showConflicts() const;

  void setConflictsAvailable(bool available);
  void setExcludedCount(int count);
  void setUndoEnabled(bool enabled);

 signals:
  void filterChanged();
  void resetExclusionsRequested();
  void undoRequested();
  void unchangedToggled(bool enabled);

 private:
  QCheckBox* excluded_ = nullptr;
  QCheckBox* directories_ = nullptr;
  QVector<QCheckBox*> actions_;
  QCheckBox* unchanged_ = nullptr;
  QCheckBox* conflicts_ = nullptr;
  QLabel* excludedCount_ = nullptr;
  QToolButton* undo_ = nullptr;
};

}  // namespace qt_rsync::ui
