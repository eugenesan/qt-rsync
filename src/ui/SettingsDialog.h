#pragma once

#include <QDialog>

class QCheckBox;

namespace qt_rsync::ui {

class SettingsDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private slots:
  void save();

 private:
  QCheckBox* sound_ = nullptr;
  QCheckBox* timing_ = nullptr;
  QCheckBox* debug_ = nullptr;
  QCheckBox* preserve_ = nullptr;
  QCheckBox* verification_ = nullptr;
  QCheckBox* verificationRsync_ = nullptr;
};

}  // namespace qt_rsync::ui
