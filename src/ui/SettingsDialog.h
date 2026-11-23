#pragma once

#include <QDialog>

namespace qt_rsync::ui {

// Placeholder for the General / Logging / Backup / Tools / Advanced pages.
class SettingsDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SettingsDialog(QWidget* parent = nullptr);
};

}  // namespace qt_rsync::ui
