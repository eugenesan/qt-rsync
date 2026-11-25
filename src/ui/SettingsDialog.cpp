#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#include "core/Paths.h"
#include "core/Storage.h"

namespace qt_rsync::ui {
namespace {

struct ExternalTool {
  QString name;
  QString command;
  QString options;
  bool enabled = true;
};

QJsonObject toolToJson(const ExternalTool& tool) {
  QJsonObject object;
  object.insert(QStringLiteral("name"), tool.name);
  object.insert(QStringLiteral("command"), tool.command);
  object.insert(QStringLiteral("options"), tool.options);
  object.insert(QStringLiteral("enabled"), tool.enabled);
  return object;
}

ExternalTool toolFromJson(const QJsonObject& object) {
  ExternalTool tool;
  tool.name = object.value(QStringLiteral("name")).toString();
  tool.command = object.value(QStringLiteral("command")).toString();
  tool.options = object.value(QStringLiteral("options")).toString();
  tool.enabled = object.value(QStringLiteral("enabled")).toBool(true);
  return tool;
}

class ExternalToolDialog : public QDialog {
 public:
  ExternalToolDialog(QWidget* parent, const ExternalTool* tool)
      : QDialog(parent) {
    setWindowTitle(QStringLiteral("External tool"));
    resize(560, 460);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QVBoxLayout();
    const auto addField = [&](const QString& label, const QString& placeholder,
                              QLineEdit** edit) {
      form->addWidget(new QLabel(label, this));
      *edit = new QLineEdit(this);
      (*edit)->setPlaceholderText(placeholder);
      form->addWidget(*edit);
    };
    addField(QStringLiteral("Name"), QStringLiteral("Kate"), &name_);
    addField(QStringLiteral("Command"), QStringLiteral("kate %d/%f"),
             &command_);
    addField(QStringLiteral("Options"), QStringLiteral("options for the tool"),
             &options_);
    layout->addLayout(form);
    enabled_ = new QCheckBox(
        QStringLiteral("Offer the tool in the context menu"), this);
    enabled_->setChecked(true);
    layout->addWidget(enabled_);

    auto* helpBox = new QGroupBox(QStringLiteral("Placeholders"), this);
    auto* helpLayout = new QVBoxLayout(helpBox);
    auto* help = new QLabel(
        QStringLiteral(
            "%a  full path in the picked pane\n"
            "%f  file name in the picked pane\n"
            "%x  extension of that file name\n"
            "%b  file name without its extension\n"
            "%d  directory in the picked pane\n"
            "%A  full path in the other pane\n"
            "%F  file name in the other pane\n"
            "%D  directory in the other pane\n"
            "%T  name of the tool\n"
            "%O  options of the tool\n"
            "%%  a literal %"),
        helpBox);
    help->setTextFormat(Qt::PlainText);
    help->setWordWrap(true);
    helpLayout->addWidget(help);
    layout->addWidget(helpBox, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    if (tool != nullptr) {
      name_->setText(tool->name);
      command_->setText(tool->command);
      options_->setText(tool->options);
      enabled_->setChecked(tool->enabled);
    }
  }

  ExternalTool tool() const {
    return {name_->text().trimmed(), command_->text().trimmed(),
            options_->text().trimmed(), enabled_->isChecked()};
  }

 private:
  QLineEdit* name_ = nullptr;
  QLineEdit* command_ = nullptr;
  QLineEdit* options_ = nullptr;
  QCheckBox* enabled_ = nullptr;
};

}  // namespace

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

  auto* coloursBox = new QGroupBox(QStringLiteral("Decision colours"), general);
  auto* coloursLayout = new QVBoxLayout(coloursBox);
  auto* coloursHint = new QLabel(
      QStringLiteral(
          "The colour marks the items of a decision in the list. Switch the "
          "marking off to leave a decision in the plain colour."),
      coloursBox);
  coloursHint->setWordWrap(true);
  coloursLayout->addWidget(coloursHint);
  colours_ = new QTableWidget(0, 3, coloursBox);
  colours_->setHorizontalHeaderLabels(
      {QStringLiteral("Decision"), QStringLiteral("Colour"), QStringLiteral("Marked")});
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
        QStringLiteral("Decision colour"));
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
  sessionLogging_ = new QCheckBox(QStringLiteral("Enable logging"), logging);
  sessionLogging_->setChecked(
      settings.value(QStringLiteral("logging/enabled"), true).toBool());
  loggingForm->addRow(QString(), sessionLogging_);
  logFile_ = new QLineEdit(logging);
  logFile_->setText(
      settings.value(QStringLiteral("logging/file"),
                     core::paths::appLogPath(QStringLiteral("default")))
          .toString());
  loggingForm->addRow(QStringLiteral("Log file"), logFile_);
  logBrowse_ = new QPushButton(QStringLiteral("Browse..."), logging);
  loggingForm->addRow(QString(), logBrowse_);
  auto* loggingNote = new QLabel(
      QStringLiteral(
          "The journal is written to this file. The toolbar button opens it."),
      logging);
  loggingNote->setWordWrap(true);
  loggingForm->addRow(QString(), loggingNote);
  connect(logBrowse_, &QPushButton::clicked, this, [this]() {
    const QString chosen = QFileDialog::getSaveFileName(
        this, QStringLiteral("Choose log file"), logFile_->text());
    if (!chosen.isEmpty()) logFile_->setText(chosen);
  });
  connect(sessionLogging_, &QCheckBox::toggled, this, [this](bool enabled) {
    logFile_->setEnabled(enabled);
    logBrowse_->setEnabled(enabled);
  });
  logFile_->setEnabled(sessionLogging_->isChecked());
  logBrowse_->setEnabled(sessionLogging_->isChecked());
  tabs->addTab(logging, QStringLiteral("Logging"));

  auto* tools = new QWidget(tabs);
  auto* toolsLayout = new QVBoxLayout(tools);
  auto* toolsCaption = new QLabel(
      QStringLiteral("Tools that the context menu offers on an item:"), tools);
  toolsCaption->setWordWrap(true);
  toolsLayout->addWidget(toolsCaption);
  tools_ = new QTableWidget(0, 4, tools);
  tools_->setHorizontalHeaderLabels(
      {QStringLiteral("Name"), QStringLiteral("Command"),
       QStringLiteral("Options"), QStringLiteral("Enabled")});
  tools_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tools_->verticalHeader()->setVisible(false);
  tools_->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  tools_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  tools_->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  tools_->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  toolsLayout->addWidget(tools_, 1);
  auto* toolButtons = new QHBoxLayout();
  auto* addTool = new QPushButton(QStringLiteral("Add..."), tools);
  auto* editTool = new QPushButton(QStringLiteral("Edit..."), tools);
  auto* removeTool = new QPushButton(QStringLiteral("Remove"), tools);
  toolButtons->addWidget(addTool);
  toolButtons->addWidget(editTool);
  toolButtons->addWidget(removeTool);
  toolButtons->addStretch(1);
  toolsLayout->addLayout(toolButtons);
  auto* diffCaption = new QLabel(
      QStringLiteral("Diff tool command (empty uses the found tool):"), tools);
  diffCaption->setWordWrap(true);
  toolsLayout->addWidget(diffCaption);
  diffCommand_ = new QLineEdit(tools);
  diffCommand_->setPlaceholderText(QStringLiteral("meld %a %A"));
  diffCommand_->setText(
      settings.value(QStringLiteral("tools/diff-command")).toString());
  toolsLayout->addWidget(diffCommand_);

  const auto setTools = [this](const QVector<ExternalTool>& tools) {
    tools_->setRowCount(0);
    for (const ExternalTool& tool : tools) {
      const int row = tools_->rowCount();
      tools_->insertRow(row);
      tools_->setItem(row, 0, new QTableWidgetItem(tool.name));
      tools_->setItem(row, 1, new QTableWidgetItem(tool.command));
      tools_->setItem(row, 2, new QTableWidgetItem(tool.options));
      auto* enabled = new QTableWidgetItem;
      enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                        Qt::ItemIsUserCheckable);
      enabled->setCheckState(tool.enabled ? Qt::Checked : Qt::Unchecked);
      tools_->setItem(row, 3, enabled);
    }
  };
  const auto tableTools = [this]() {
    QVector<ExternalTool> result;
    for (int row = 0; row < tools_->rowCount(); ++row) {
      ExternalTool tool;
      tool.name = tools_->item(row, 0)->text();
      tool.command = tools_->item(row, 1)->text();
      tool.options = tools_->item(row, 2)->text();
      tool.enabled = tools_->item(row, 3) != nullptr &&
                     tools_->item(row, 3)->checkState() == Qt::Checked;
      result.append(tool);
    }
    return result;
  };

  QVector<ExternalTool> loadedTools;
  const QJsonArray storedTools =
      QJsonDocument::fromJson(
          settings.value(QStringLiteral("tools/external")).toByteArray())
          .array();
  for (const QJsonValue& value : storedTools) {
    const ExternalTool tool = toolFromJson(value.toObject());
    if (!tool.name.isEmpty() && !tool.command.isEmpty()) loadedTools.append(tool);
  }
  if (loadedTools.isEmpty()) {
    for (const QString& command :
         settings.value(QStringLiteral("tools/commands")).toStringList()) {
      if (command.trimmed().isEmpty()) continue;
      loadedTools.append({command.section(' ', 0, 0), command, {}, true});
    }
  }
  setTools(loadedTools);

  connect(addTool, &QPushButton::clicked, tools, [this, setTools, tableTools]() {
    ExternalToolDialog dialog(this, nullptr);
    if (dialog.exec() != QDialog::Accepted) return;
    const ExternalTool tool = dialog.tool();
    if (tool.name.isEmpty() || tool.command.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("External tool"),
                           QStringLiteral("A tool needs a name and a command."));
      return;
    }
    setTools(tableTools() + QVector<ExternalTool>{tool});
  });
  const auto editSelected = [this, setTools, tableTools]() {
    const int row = tools_->currentRow();
    QVector<ExternalTool> list = tableTools();
    if (row < 0 || row >= list.size()) return;
    ExternalToolDialog dialog(this, &list[row]);
    if (dialog.exec() != QDialog::Accepted) return;
    const ExternalTool tool = dialog.tool();
    if (tool.name.isEmpty() || tool.command.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("External tool"),
                           QStringLiteral("A tool needs a name and a command."));
      return;
    }
    list[row] = tool;
    setTools(list);
  };
  connect(editTool, &QPushButton::clicked, tools, editSelected);
  connect(tools_, &QTableWidget::cellDoubleClicked, tools,
          [editSelected](int, int) { editSelected(); });
  connect(removeTool, &QPushButton::clicked, tools,
          [this, setTools, tableTools]() {
            QVector<ExternalTool> list = tableTools();
            QList<int> rows;
            for (const QModelIndex& index : tools_->selectionModel()->selectedRows()) {
              rows.append(index.row());
            }
            std::sort(rows.begin(), rows.end(), std::greater<int>());
            for (int row : rows) {
              if (row >= 0 && row < list.size()) list.removeAt(row);
            }
            setTools(list);
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

  auto* buttonRow = new QHBoxLayout();
  auto* defaults = new QPushButton(QStringLiteral("Defaults"), this);
  buttonRow->addWidget(defaults);
  buttonRow->addStretch(1);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
  buttonRow->addWidget(buttons);
  layout->addLayout(buttonRow);
  connect(defaults, &QPushButton::clicked, this,
          [this, defaultColours]() {
            sound_->setChecked(true);
            sessionLogging_->setChecked(true);
            verification_->setChecked(false);
            verificationRsync_->setChecked(false);
            detectInherited_->setChecked(false);
            rawRsync_->setChecked(false);
            twoPass_->setChecked(false);
            inventoryPreload_->setChecked(false);
            timing_->setChecked(false);
            debug_->setChecked(false);
            for (int row = 0; row < defaultColours.size(); ++row) {
              const QString colour = defaultColours.at(row).second;
              colours_->item(row, 1)->setText(colour);
              colours_->item(row, 1)->setBackground(QColor(colour));
              colours_->item(row, 2)->setCheckState(Qt::Checked);
            }
          });
}

void SettingsDialog::save() {
  core::storage::Settings settings;
  settings.setValue(QStringLiteral("sound/enabled"), sound_->isChecked());
  QJsonObject colours;
  for (int row = 0; row < colours_->rowCount(); ++row) {
    const bool marked =
        colours_->item(row, 2) != nullptr &&
        colours_->item(row, 2)->checkState() == Qt::Checked;
    colours.insert(colours_->item(row, 0)->text(),
                   marked ? QJsonValue(colours_->item(row, 1)->text())
                          : QJsonValue(QJsonValue::Null));
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
  settings.setValue(QStringLiteral("logging/file"), logFile_->text().trimmed());
  QJsonArray tools;
  QStringList commands;
  for (int row = 0; row < tools_->rowCount(); ++row) {
    ExternalTool tool;
    tool.name = tools_->item(row, 0)->text();
    tool.command = tools_->item(row, 1)->text();
    tool.options = tools_->item(row, 2)->text();
    tool.enabled = tools_->item(row, 3) != nullptr &&
                   tools_->item(row, 3)->checkState() == Qt::Checked;
    tools.append(toolToJson(tool));
    commands << tool.command;
  }
  settings.setValue(QStringLiteral("tools/external"),
                    QJsonDocument(tools).toJson(QJsonDocument::Compact));
  settings.setValue(QStringLiteral("tools/diff-command"),
                    diffCommand_->text().trimmed());
  settings.setValue(QStringLiteral("tools/commands"), commands);
  settings.setValue(QStringLiteral("decisions/detect-inherited"), detectInherited_->isChecked());
  settings.setValue(QStringLiteral("logging/rsync_output"), rawRsync_->isChecked());
  settings.setValue(QStringLiteral("advanced/dest_metadata_two_pass"), twoPass_->isChecked());
  settings.setValue(QStringLiteral("advanced/inventory_preload"), inventoryPreload_->isChecked());
  settings.sync();
}

}  // namespace qt_rsync::ui
