#include "ui/ProfileEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>

#include <functional>

#include "core/Filter.h"

namespace qt_rsync::ui {
namespace {

QCheckBox* addCheck(QVBoxLayout* layout, const QString& text, bool checked,
                    const std::function<void(bool)>& changed) {
  auto* box = new QCheckBox(text);
  box->setChecked(checked);
  QObject::connect(box, &QCheckBox::toggled, box, changed);
  layout->addWidget(box);
  return box;
}

}  // namespace

ProfileEditorDialog::ProfileEditorDialog(const core::SyncProfile& profile,
                                         QWidget* parent,
                                         const QString& section)
    : QDialog(parent), profile_(profile) {
  setWindowTitle(QStringLiteral("Profile"));
  resize(640, 620);
  auto* layout = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  layout->addWidget(tabs, 1);

  auto* options = new QWidget(tabs);
  auto* optionsLayout = new QVBoxLayout(options);
  auto* modeRow = new QFormLayout();
  auto* mode = new QComboBox(options);
  mode->addItem(QStringLiteral("Update"), static_cast<int>(core::SyncMode::Update));
  mode->addItem(QStringLiteral("Mirror"), static_cast<int>(core::SyncMode::Mirror));
  mode->addItem(QStringLiteral("New Only"),
                static_cast<int>(core::SyncMode::NewOnly));
  mode->setCurrentIndex(static_cast<int>(profile.transfer.mode));
  modeRow->addRow(QStringLiteral("Method"), mode);
  optionsLayout->addLayout(modeRow);
  connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this, mode](int index) {
            profile_.transfer.mode =
                static_cast<core::SyncMode>(mode->itemData(index).toInt());
          });
  addCheck(optionsLayout, QStringLiteral("Archive mode"),
           profile.preservation.archive, [this](bool on) {
             profile_.preservation.archive = on;
           });
  addCheck(optionsLayout, QStringLiteral("Compress data during transfer"),
           profile.transfer.compress, [this](bool on) {
             profile_.transfer.compress = on;
           });
  addCheck(optionsLayout, QStringLiteral("Compare files by checksum"),
           profile.transfer.checksum, [this](bool on) {
             profile_.transfer.checksum = on;
           });
  addCheck(optionsLayout, QStringLiteral("Update files in place"),
           profile.transfer.inplace, [this](bool on) {
             profile_.transfer.inplace = on;
           });
  addCheck(optionsLayout, QStringLiteral("Use fuzzy transfers"),
           profile.transfer.fuzzy, [this](bool on) {
             profile_.transfer.fuzzy = on;
           });
  addCheck(optionsLayout, QStringLiteral("Use whole-file transfers"),
           profile.transfer.wholeFile, [this](bool on) {
             profile_.transfer.wholeFile = on;
           });
  addCheck(optionsLayout, QStringLiteral("Copy symlinks as copies"),
           profile.transfer.copyLinks, [this](bool on) {
             profile_.transfer.copyLinks = on;
           });
  addCheck(optionsLayout,
           QStringLiteral("Do not cross file-system boundaries"),
           profile.transfer.oneFileSystem, [this](bool on) {
             profile_.transfer.oneFileSystem = on;
           });
  addCheck(optionsLayout, QStringLiteral("Ignore modification times"),
           profile.transfer.ignoreTimes, [this](bool on) {
             profile_.transfer.ignoreTimes = on;
           });
  addCheck(optionsLayout, QStringLiteral("Transfer sparse files efficiently"),
           profile.transfer.sparse, [this](bool on) {
             profile_.transfer.sparse = on;
           });
  addCheck(optionsLayout,
           QStringLiteral("Run an inventory after every comparison"),
           profile.inventoryAfterCompare, [this](bool on) {
             profile_.inventoryAfterCompare = on;
           });
  auto* partialRow = new QFormLayout();
  auto* partialEdit = new QLineEdit(profile.transfer.partialDir, options);
  partialEdit->setPlaceholderText(
      QStringLiteral("Empty uses rsync's temporary files"));
  partialRow->addRow(QStringLiteral("Partial directory"), partialEdit);
  optionsLayout->addLayout(partialRow);
  connect(partialEdit, &QLineEdit::textChanged, this,
          [this](const QString& text) { profile_.transfer.partialDir = text; });
  optionsLayout->addStretch(1);
  tabs->addTab(options, QStringLiteral("Options"));

  auto* preservation = new QWidget(tabs);
  auto* preservationLayout = new QVBoxLayout(preservation);
  addCheck(preservationLayout, QStringLiteral("Preserve hard links"),
           profile.preservation.hardLinks, [this](bool on) {
             profile_.preservation.hardLinks = on;
           });
  addCheck(preservationLayout, QStringLiteral("Preserve ACLs"),
           profile.preservation.acls, [this](bool on) {
             profile_.preservation.acls = on;
           });
  addCheck(preservationLayout, QStringLiteral("Preserve extended attributes"),
           profile.preservation.xattrs, [this](bool on) {
             profile_.preservation.xattrs = on;
           });
  addCheck(preservationLayout, QStringLiteral("Keep numeric IDs"),
           profile.preservation.numericIds, [this](bool on) {
             profile_.preservation.numericIds = on;
           });
  preservationLayout->addStretch(1);
  tabs->addTab(preservation, QStringLiteral("Preservation"));

  auto* backup = new QWidget(tabs);
  auto* backupLayout = new QVBoxLayout(backup);
  auto* backupRow = new QFormLayout();
  auto* backupEdit = new QLineEdit(profile.backup.directory, backup);
  backupEdit->setPlaceholderText(QStringLiteral("Empty disables backups"));
  backupRow->addRow(QStringLiteral("Backup directory"), backupEdit);
  backupLayout->addLayout(backupRow);
  connect(backupEdit, &QLineEdit::textChanged, this,
          [this](const QString& text) { profile_.backup.directory = text; });
  addCheck(backupLayout, QStringLiteral("Protect nested backup directory"),
           profile.backup.protectNested, [this](bool on) {
             profile_.backup.protectNested = on;
           });
  auto* backupHint = new QLabel(
      QStringLiteral(
          "The backup directory is excluded from synchronization and protected "
          "from deletion when it lies inside Source or Destination."),
      backup);
  backupHint->setWordWrap(true);
  backupLayout->addWidget(backupHint);
  backupLayout->addStretch(1);
  tabs->addTab(backup, QStringLiteral("Backup"));

  auto* filters = new QWidget(tabs);
  auto* filtersLayout = new QVBoxLayout(filters);
  auto* filterEdit = new QPlainTextEdit(profile.filterText, filters);
  filterEdit->setPlaceholderText(
      QStringLiteral("One rsync filter rule per line, for example:\n"
                     "- *.tmp\n"
                     "+ important/\n"
                     "- *"));
  filtersLayout->addWidget(filterEdit, 1);
  connect(filterEdit, &QPlainTextEdit::textChanged, this,
          [this, filterEdit]() {
            profile_.filterText = filterEdit->toPlainText();
            profile_.filters = core::filterTextToRules(profile_.filterText);
          });
  tabs->addTab(filters, QStringLiteral("Filters"));

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  if (section == QLatin1String("backup")) {
    tabs->setCurrentWidget(backup);
  } else if (section == QLatin1String("filters")) {
    tabs->setCurrentWidget(filters);
  } else if (section == QLatin1String("preservation")) {
    tabs->setCurrentWidget(preservation);
  }
}

}  // namespace qt_rsync::ui
