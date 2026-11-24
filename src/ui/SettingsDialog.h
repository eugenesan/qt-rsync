#pragma once

#include <QDialog>

class QListWidget;

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
  QCheckBox* sessionLogging_ = nullptr;
  QListWidget* tools_ = nullptr;
  QCheckBox* detectInherited_ = nullptr;
  QCheckBox* rawRsync_ = nullptr;
  QCheckBox* twoPass_ = nullptr;
  QCheckBox* inventoryPreload_ = nullptr;
};

}  // namespace qt_rsync::ui
