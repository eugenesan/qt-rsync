#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QListWidget>
#include <QPushButton>
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
  sessionLogging_ = new QCheckBox(
      QStringLiteral("Write a session log to disk"), logging);
  sessionLogging_->setChecked(
      settings.value(QStringLiteral("logging/enabled"), true).toBool());
  loggingForm->addRow(QString(), sessionLogging_);
  loggingForm->addRow(QStringLiteral("Session log:"),
                      new QLabel(core::paths::appLogPath(QStringLiteral("default")), logging));
  tabs->addTab(logging, QStringLiteral("Logging"));

  auto* tools = new QWidget(tabs);
  auto* toolsLayout = new QVBoxLayout(tools);
  tools_ = new QListWidget(tools);
  for (const QString& command :
       settings.value(QStringLiteral("tools/commands")).toStringList()) {
    tools_->addItem(command);
  }
  toolsLayout->addWidget(tools_);
  auto* toolButtons = new QHBoxLayout();
  auto* addTool = new QPushButton(QStringLiteral("Add..."), tools);
  auto* removeTool = new QPushButton(QStringLiteral("Remove"), tools);
  toolButtons->addWidget(addTool);
  toolButtons->addWidget(removeTool);
  toolButtons->addStretch(1);
  toolsLayout->addLayout(toolButtons);
  connect(addTool, &QPushButton::clicked, tools, [this]() {
    bool ok = false;
    const QString command = QInputDialog::getText(
        this, QStringLiteral("External tool"), QStringLiteral("Command:"),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !command.trimmed().isEmpty()) tools_->addItem(command.trimmed());
  });
  connect(removeTool, &QPushButton::clicked, tools, [this]() {
    for (QListWidgetItem* item : tools_->selectedItems()) delete item;
  });
  tabs->addTab(tools, QStringLiteral("Tools"));

  auto* advanced = new QWidget(tabs);
  auto* advancedForm = new QFormLayout(advanced);
  preserve_ = new QCheckBox(QStringLiteral("Preserve decisions across compares"), advanced);
  preserve_->setChecked(
      settings.value(QStringLiteral("decisions/preserve"), true).toBool());
  advancedForm->addRow(QString(), preserve_);
  verification_ = new QCheckBox(
      QStringLiteral("Enable Verify Decisions (F8)"), advanced);
  verificationRsync_ = new QCheckBox(
      QStringLiteral("Use an rsync dry run when verifying"), advanced);
  verification_->setChecked(
      settings.value(QStringLiteral("verification/enabled"), true).toBool());
  verificationRsync_->setChecked(
      settings.value(QStringLiteral("verification/rsync"), true).toBool());
  advancedForm->addRow(QString(), verification_);
  advancedForm->addRow(QString(), verificationRsync_);
  detectInherited_ = new QCheckBox(
      QStringLiteral("Flag new destination-only items below included folders"), advanced);
  rawRsync_ = new QCheckBox(QStringLiteral("Log raw rsync output"), advanced);
  twoPass_ = new QCheckBox(
      QStringLiteral("Use two rsync runs for destination metadata"), advanced);
  inventoryPreload_ = new QCheckBox(
      QStringLiteral("Preload the source listing for unchanged items"), advanced);
  detectInherited_->setChecked(settings.value(QStringLiteral("decisions/detect-inherited"), false).toBool());
  rawRsync_->setChecked(settings.value(QStringLiteral("logging/rsync_output"), false).toBool());
  twoPass_->setChecked(settings.value(QStringLiteral("advanced/dest_metadata_two_pass"), false).toBool());
  inventoryPreload_->setChecked(settings.value(QStringLiteral("advanced/inventory_preload"), false).toBool());
  advancedForm->addRow(QString(), detectInherited_);
  advancedForm->addRow(QString(), rawRsync_);
  advancedForm->addRow(QString(), twoPass_);
  advancedForm->addRow(QString(), inventoryPreload_);
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
  settings.setValue(QStringLiteral("verification/enabled"),
                    verification_->isChecked());
  settings.setValue(QStringLiteral("verification/rsync"),
                    verificationRsync_->isChecked());
  settings.setValue(QStringLiteral("logging/enabled"),
                    sessionLogging_->isChecked());
  QStringList commands;
  for (int row = 0; row < tools_->count(); ++row) {
    commands << tools_->item(row)->text();
  }
  settings.setValue(QStringLiteral("tools/commands"), commands);
  settings.setValue(QStringLiteral("decisions/detect-inherited"), detectInherited_->isChecked());
  settings.setValue(QStringLiteral("logging/rsync_output"), rawRsync_->isChecked());
  settings.setValue(QStringLiteral("advanced/dest_metadata_two_pass"), twoPass_->isChecked());
  settings.setValue(QStringLiteral("advanced/inventory_preload"), inventoryPreload_->isChecked());
  settings.sync();
}

}  // namespace qt_rsync::ui
