#pragma once

#include <QDialog>

#include "core/Profile.h"

namespace qt_rsync::ui {

// Edits the transfer, preservation, backup and filter options of a profile.
// The Source and Destination endpoints stay in the main window, as in qpy.
class ProfileEditorDialog : public QDialog {
  Q_OBJECT
 public:
  explicit ProfileEditorDialog(const core::SyncProfile& profile,
                               QWidget* parent = nullptr,
                               const QString& section = QStringLiteral("options"));

  [[nodiscard]] core::SyncProfile profile() const { return profile_; }

 private:
  core::SyncProfile profile_;
};

}  // namespace qt_rsync::ui
