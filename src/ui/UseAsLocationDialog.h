#pragma once

#include <QDialog>

class QCheckBox;

namespace qt_rsync::ui {

// Chooses which endpoint should be narrowed to a comparison folder. The
// dialog never changes the profile itself; the caller applies the choice.
class UseAsLocationDialog : public QDialog {
  Q_OBJECT
 public:
  explicit UseAsLocationDialog(const QString& relativePath,
                               const QString& sourceRoot,
                               const QString& destinationRoot,
                               QWidget* parent = nullptr);

  [[nodiscard]] bool updateFilters() const;

 private:
  QCheckBox* updateFilters_ = nullptr;
};

}  // namespace qt_rsync::ui
