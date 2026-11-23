#include "ui/SettingsDialog.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace qt_rsync::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Settings"));
  resize(720, 600);
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this));
}

}  // namespace qt_rsync::ui
