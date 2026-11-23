#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/Paths.h"
#include "core/Storage.h"

namespace qt_rsync::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Settings"));
  resize(720, 480);
  core::storage::Settings settings;

  auto* layout = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  layout->addWidget(tabs);

  auto* general = new QWidget(tabs);
  auto* generalForm = new QFormLayout(general);
  sound_ = new QCheckBox(QStringLiteral("Play completion sound"), general);
  sound_->setChecked(settings.value(QStringLiteral("sound/enabled"), true).toBool());
  generalForm->addRow(QString(), sound_);
  tabs->addTab(general, QStringLiteral("General"));

  auto* logging = new QWidget(tabs);
  auto* loggingForm = new QFormLayout(logging);
  timing_ = new QCheckBox(
      QStringLiteral("Include timing diagnostics in the session log"), logging);
  debug_ = new QCheckBox(
      QStringLiteral("Include rsync debug diagnostics in the session log"), logging);
  timing_->setChecked(settings.value(QStringLiteral("logging/timing"), false).toBool());
  debug_->setChecked(settings.value(QStringLiteral("logging/debug"), false).toBool());
  loggingForm->addRow(QString(), timing_);
  loggingForm->addRow(QString(), debug_);
  loggingForm->addRow(QStringLiteral("Session log:"),
                      new QLabel(core::paths::appLogPath(QStringLiteral("default")), logging));
  tabs->addTab(logging, QStringLiteral("Logging"));

  auto* tools = new QLabel(
      QStringLiteral("External tools are configured in the profile file."), tabs);
  tabs->addTab(tools, QStringLiteral("Tools"));

  auto* advanced = new QWidget(tabs);
  auto* advancedForm = new QFormLayout(advanced);
  preserve_ = new QCheckBox(QStringLiteral("Preserve decisions across compares"), advanced);
  preserve_->setChecked(
      settings.value(QStringLiteral("decisions/preserve"), true).toBool());
  advancedForm->addRow(QString(), preserve_);
  tabs->addTab(advanced, QStringLiteral("Advanced"));

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
  layout->addWidget(buttons);
}

void SettingsDialog::save() {
  core::storage::Settings settings;
  settings.setValue(QStringLiteral("sound/enabled"), sound_->isChecked());
  settings.setValue(QStringLiteral("logging/timing"), timing_->isChecked());
  settings.setValue(QStringLiteral("logging/debug"), debug_->isChecked());
  settings.setValue(QStringLiteral("decisions/preserve"), preserve_->isChecked());
  settings.sync();
}

}  // namespace qt_rsync::ui
