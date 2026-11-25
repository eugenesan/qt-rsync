#pragma once

#include <QHash>
#include <QWidget>

class QCheckBox;
class QEvent;
class QPoint;
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
  void setActionColours(const QVector<QString>& colours);
  void setExcludedCount(int count);
  void toggleUnchanged();
  bool showUnchanged() const;
  void setFilterDirectories(bool enabled);
  void setUndoEnabled(bool enabled);

  bool eventFilter(QObject* watched, QEvent* event) override;

 signals:
  void filterChanged();
  void resetExclusionsRequested();
  void undoRequested();
  void unchangedToggled(bool enabled);

 private:
  QVector<QCheckBox*> allBoxes() const;
  void applyInverse(QCheckBox* box);
  void setStates(const QHash<QCheckBox*, bool>& wanted);
  void resetDefaults();
  void showBoxMenu(QCheckBox* box, const QPoint& point);
  void showExcludedMenu(const QPoint& point);

  QCheckBox* excluded_ = nullptr;
  QCheckBox* directories_ = nullptr;
  QVector<QCheckBox*> actions_;
  QCheckBox* unchanged_ = nullptr;
  QCheckBox* conflicts_ = nullptr;
  QToolButton* undo_ = nullptr;
  QCheckBox* inverseClick_ = nullptr;
  int excludedCount_ = 0;
};

}  // namespace qt_rsync::ui
