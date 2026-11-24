#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTableWidget>
#include <QInputDialog>
#include <QListWidget>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/Paths.h"
#include "core/Storage.h"

namespace qt_rsync::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Settings"));
  resize(720, 600);
  core::storage::Settings settings;

  auto* layout = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  layout->addWidget(tabs);

  auto* general = new QWidget(tabs);
  auto* generalLayout = new QVBoxLayout(general);
  sound_ = new QCheckBox(
      QStringLiteral("Play a sound when a comparison or a sync finishes"),
      general);
  sound_->setChecked(settings.value(QStringLiteral("sound/enabled"), true).toBool());
  sound_->setToolTip(
      QStringLiteral(
          "Uses the desktop sound when one is installed and the terminal bell "
          "otherwise."));
  generalLayout->addWidget(sound_);
  auto* generalHint = new QLabel(
      QStringLiteral(
          "Logging and backup configuration live in their own tabs. The "
          "toolbar keeps a button that opens each location."),
      general);
  generalHint->setWordWrap(true);
  generalLayout->addWidget(generalHint);

  auto* coloursBox = new QGroupBox(QStringLiteral("Action colours"), general);
  auto* coloursLayout = new QVBoxLayout(coloursBox);
  auto* coloursHint = new QLabel(
      QStringLiteral(
          "The colour marks the items of an action in the list. Switch the "
          "marking off to leave an action in the plain colour."),
      coloursBox);
  coloursHint->setWordWrap(true);
  coloursLayout->addWidget(coloursHint);
  colours_ = new QTableWidget(0, 3, coloursBox);
  colours_->setHorizontalHeaderLabels(
      {QStringLiteral("Action"), QStringLiteral("Colour"), QStringLiteral("Marked")});
  colours_->verticalHeader()->setVisible(false);
  colours_->setSelectionBehavior(QAbstractItemView::SelectRows);
  colours_->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  colours_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  colours_->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  const QVector<QPair<QString, QString>> defaultColours{
      {QStringLiteral("Create"), QStringLiteral("#208020")},
      {QStringLiteral("Update"), QStringLiteral("#eab308")},
      {QStringLiteral("Metadata only"), QStringLiteral("#7c3aed")},
      {QStringLiteral("Delete"), QStringLiteral("#ef4444")},
      {QStringLiteral("Keep in Destination"), QStringLiteral("#a05020")},
  };
  QJsonObject stored = QJsonDocument::fromJson(
      settings.value(QStringLiteral("colours/actions")).toByteArray()).object();
  colours_->setRowCount(defaultColours.size());
  for (int row = 0; row < defaultColours.size(); ++row) {
    const QString colour = stored.value(defaultColours.at(row).first).toString(
        defaultColours.at(row).second);
    colours_->setItem(row, 0, new QTableWidgetItem(defaultColours.at(row).first));
    colours_->setItem(row, 1, new QTableWidgetItem(colour));
    colours_->item(row, 1)->setBackground(QColor(colour));
    auto* marked = new QTableWidgetItem;
    marked->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    marked->setCheckState(Qt::Checked);
    colours_->setItem(row, 2, marked);
  }
  connect(colours_, &QTableWidget::cellClicked, general, [this](int row, int column) {
    if (column != 1) return;
    const QColor chosen = QColorDialog::getColor(
        QColor(colours_->item(row, 1)->text()), this,
        QStringLiteral("Action colour"));
    if (!chosen.isValid()) return;
    colours_->item(row, 1)->setText(chosen.name());
    colours_->item(row, 1)->setBackground(chosen);
  });
  coloursLayout->addWidget(colours_);
  generalLayout->addWidget(coloursBox, 1);
  generalLayout->addStretch(0);
  tabs->addTab(general, QStringLiteral("General"));

  auto* logging = new QWidget(tabs);
  auto* loggingForm = new QFormLayout(logging);
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
  auto* advancedLayout = new QVBoxLayout(advanced);
  verification_ = new QCheckBox(
      QStringLiteral("Show the Verify Decisions check"), advanced);
  verification_->setChecked(
      settings.value(QStringLiteral("verification/enabled"), false).toBool());
  verification_->setToolTip(
      QStringLiteral(
          "Off by default. Shows an optional consistency check between the "
          "selected decisions and the execution plan. Synchronization does "
          "not depend on it."));
  advancedLayout->addWidget(verification_);
  verificationRsync_ = new QCheckBox(
      QStringLiteral("Verify with a second rsync dry run"), advanced);
  verificationRsync_->setChecked(
      settings.value(QStringLiteral("verification/rsync"), false).toBool());
  verificationRsync_->setToolTip(
      QStringLiteral(
          "Off by default. Re-scans the live tree and reports differences "
          "from the reviewed comparison as a warning. On a changing "
          "directory this will report unrelated changes; re-compare close "
          "to verification to reduce them."));
  advancedLayout->addWidget(verificationRsync_);
  detectInherited_ = new QCheckBox(
      QStringLiteral(
          "Flag new destination-only items below included folders"),
      advanced);
  detectInherited_->setChecked(
      settings.value(QStringLiteral("decisions/detect-inherited"), false).toBool());
  detectInherited_->setToolTip(
      QStringLiteral(
          "Off by default. When a folder has an INCLUDE decision, a new "
          "destination-only item below it would be deleted. This check flags "
          "that decision for review.\n"
          "A folder-wide INCLUDE is often a deliberate choice to own "
          "everything below it: rotating logs under /var/log, for example, "
          "would produce a review on every sync. Leave off unless those "
          "deletions should be confirmed explicitly."));
  advancedLayout->addWidget(detectInherited_);
  rawRsync_ = new QCheckBox(QStringLiteral("Log raw rsync output"), advanced);
  rawRsync_->setChecked(
      settings.value(QStringLiteral("logging/rsync_output"), false).toBool());
  rawRsync_->setToolTip(
      QStringLiteral(
          "Off by default. Writes every itemized rsync record to the rsync "
          "log file. On a large tree this can add hundreds of MB per "
          "comparison and costs measurable time. Application events and "
          "timing lines are always logged."));
  advancedLayout->addWidget(rawRsync_);
  twoPass_ = new QCheckBox(
      QStringLiteral("Use two rsync runs for destination metadata"), advanced);
  twoPass_->setChecked(
      settings.value(QStringLiteral("advanced/dest_metadata_two_pass"), false).toBool());
  twoPass_->setToolTip(
      QStringLiteral(
          "Off by default: one unfiltered rsync listing fills in destination "
          "sizes and dates after a compare. Turn on to run a filtered listing "
          "first and an unfiltered one only for paths the filters hid. This "
          "can be faster when the profile excludes very large subtrees, but "
          "normally costs an extra rsync process and SSH handshake."));
  advancedLayout->addWidget(twoPass_);
  inventoryPreload_ = new QCheckBox(
      QStringLiteral("Preload the source listing for unchanged items"),
      advanced);
  inventoryPreload_->setChecked(
      settings.value(QStringLiteral("advanced/inventory_preload"), false).toBool());
  inventoryPreload_->setToolTip(
      QStringLiteral(
          "Off by default: the source is listed only when you ask to show "
          "unchanged items. Turn on to list source and destination together "
          "after each compare, so showing unchanged needs no extra rsync. "
          "Useful when you routinely browse unchanged items, but endpoint "
          "I/O contention can cancel the benefit: two listings at once may "
          "be no faster than one, and every compare pays the extra work. "
          "It also keeps roughly a million extra entries in memory."));
  advancedLayout->addWidget(inventoryPreload_);
  timing_ = new QCheckBox(
      QStringLiteral("Include timing diagnostics in the session log"),
      advanced);
  timing_->setChecked(
      settings.value(QStringLiteral("logging/timing"), false).toBool());
  timing_->setToolTip(
      QStringLiteral(
          "Off by default. When enabled, plan, model, sort and native-view "
          "timings are written to the session log and standard error. Use "
          "this when reporting a performance problem: reproduce it, then "
          "attach the session log from "
          "$XDG_CACHE_HOME/qt-rsync/app-<profile>.log.\n\n"
          "Volume is low (a few lines per operation), so it is safe to leave "
          "on. Applies immediately; no restart needed."));
  advancedLayout->addWidget(timing_);
  debug_ = new QCheckBox(
      QStringLiteral("Include rsync debug diagnostics in the session log"),
      advanced);
  debug_->setChecked(
      settings.value(QStringLiteral("logging/debug"), false).toBool());
  debug_->setToolTip(
      QStringLiteral(
          "Off by default. When enabled, logs rsync/SSH command arguments, "
          "process exit codes, compare summaries and samples of stdout and "
          "stderr. Use for failures that timing data alone cannot explain, "
          "and attach the session log when reporting a bug. Enabling this "
          "also turns on timing diagnostics.\n\n"
          "Warning: much more verbose than timing diagnostics. Large trees or "
          "progress output can add many lines with file names, so turn it off "
          "when you are done. Applies immediately."));
  advancedLayout->addWidget(debug_);
  advancedLayout->addSpacing(24);
  auto* advancedHint = new QLabel(
      QStringLiteral(
          "Verification checks are diagnostics, not gates: the "
          "synchronization confirmation dialog remains the final review "
          "before writing, and the optional rsync check needs Verify "
          "Decisions."),
      advanced);
  advancedHint->setWordWrap(true);
  advancedLayout->addWidget(advancedHint);
  advancedLayout->addStretch(1);
  connect(verification_, &QCheckBox::toggled, verificationRsync_,
          &QWidget::setEnabled);
  connect(verificationRsync_, &QCheckBox::toggled, this, [this](bool checked) {
    if (checked && !verification_->isChecked()) verification_->setChecked(true);
  });
  verificationRsync_->setEnabled(verification_->isChecked());
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
  QJsonObject colours;
  for (int row = 0; row < colours_->rowCount(); ++row) {
    colours.insert(colours_->item(row, 0)->text(), colours_->item(row, 1)->text());
  }
  settings.setValue(QStringLiteral("colours/actions"),
                    QJsonDocument(colours).toJson(QJsonDocument::Compact));
  settings.setValue(QStringLiteral("logging/timing"), timing_->isChecked());
  settings.setValue(QStringLiteral("logging/debug"), debug_->isChecked());
  settings.setValue(QStringLiteral("verification/enabled"),
                    verification_->isChecked());
  settings.setValue(QStringLiteral("verification/rsync"),
                    verification_->isChecked() && verificationRsync_->isChecked());
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
