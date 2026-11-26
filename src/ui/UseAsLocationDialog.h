#pragma once

#include <QDialog>

class QCheckBox;

namespace qt_rsync::ui {

// Chooses which endpoint should be narrowed to a comparison folder. The
// dialog never changes the profile itself; the caller applies the choice.
class UseAsLocationDialog : public QDialog {
  Q_OBJECT
 public:
  enum class Target { Source, Destination, Both };

  explicit UseAsLocationDialog(const QString& relativePath,
                               const QString& sourceRoot,
                               const QString& destinationRoot,
                               QWidget* parent = nullptr);

  [[nodiscard]] Target target() const { return target_; }
  [[nodiscard]] bool updateFilters() const;

 private:
  Target target_ = Target::Source;
  QCheckBox* updateFilters_ = nullptr;
};

}  // namespace qt_rsync::ui
