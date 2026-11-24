#include "ui/ProfileEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBlock>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

#include "core/Filter.h"
#include "ui/Icons.h"

namespace qt_rsync::ui {
namespace {

QCheckBox* addCheck(QVBoxLayout* layout, const QString& text, bool checked,
                    const std::function<void(bool)>& changed,
                    const QString& tip = {}) {
  auto* box = new QCheckBox(text);
  box->setChecked(checked);
  if (!tip.isEmpty()) box->setToolTip(tip);
  QObject::connect(box, &QCheckBox::toggled, box, changed);
  layout->addWidget(box);
  return box;
}

void addTip(QWidget* widget, const QString& option, const QString& text) {
  widget->setToolTip(option + QLatin1Char('\n') + text);
}

QComboBox* addCombo(QFormLayout* form, const QString& label,
                    const QVector<QPair<QString, int>>& values, int current) {
  auto* combo = new QComboBox;
  for (const auto& value : values) combo->addItem(value.first, value.second);
  const int index = combo->findData(current);
  if (index >= 0) combo->setCurrentIndex(index);
  form->addRow(label, combo);
  return combo;
}

}  // namespace

ProfileEditorDialog::ProfileEditorDialog(const core::SyncProfile& profile,
                                         QWidget* parent,
                                         const QString& section)
    : QDialog(parent), profile_(profile) {
  setWindowTitle(QStringLiteral("Edit profile"));
  resize(640, 560);
  auto* layout = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  layout->addWidget(tabs, 1);

  auto* options = new QWidget(tabs);
  auto* optionsLayout = new QVBoxLayout(options);

  auto* transferGroup = new QGroupBox(QStringLiteral("Transfer"), options);
  auto* transferForm = new QFormLayout(transferGroup);
  auto* mode = addCombo(
      transferForm, QStringLiteral("Sync mode"),
      {{QStringLiteral("Update"), static_cast<int>(core::SyncMode::Update)},
       {QStringLiteral("Mirror"), static_cast<int>(core::SyncMode::Mirror)},
       {QStringLiteral("New only"), static_cast<int>(core::SyncMode::NewOnly)}},
      static_cast<int>(profile.transfer.mode));
  addTip(mode, QStringLiteral("--delete-* / --ignore-existing"),
         QStringLiteral(
             "Update transfers changes, Mirror also deletes destination-only "
             "items, New Only copies missing items and never updates."));
  connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this, mode](int index) {
            profile_.transfer.mode =
                static_cast<core::SyncMode>(mode->itemData(index).toInt());
          });
  auto* deleteTiming = addCombo(
      transferForm, QStringLiteral("Delete timing"),
      {{QStringLiteral("Before"), static_cast<int>(core::DeleteTiming::Before)},
       {QStringLiteral("During"), static_cast<int>(core::DeleteTiming::During)},
       {QStringLiteral("Delay"), static_cast<int>(core::DeleteTiming::Delay)},
       {QStringLiteral("After"), static_cast<int>(core::DeleteTiming::After)}},
      static_cast<int>(profile.transfer.deleteTiming));
  addTip(deleteTiming,
         QStringLiteral(
             "--delete-before / --delete-during / --delete-delay / --delete-after"),
         QStringLiteral(
             "When rsync removes destination-only items during a Mirror sync."));
  connect(deleteTiming, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this, deleteTiming](int index) {
            profile_.transfer.deleteTiming =
                static_cast<core::DeleteTiming>(
                    deleteTiming->itemData(index).toInt());
          });
  optionsLayout->addWidget(transferGroup);

  auto* behavioursGroup = new QGroupBox(QStringLiteral("Transfer options"),
                                        options);
  auto* behaviours = new QVBoxLayout(behavioursGroup);
  addCheck(behaviours, QStringLiteral("Compress data during transfer"),
           profile.transfer.compress,
           [this](bool on) { profile_.transfer.compress = on; },
           QStringLiteral("--compress / --no-compress\nCompress transfer data."));
  addCheck(behaviours, QStringLiteral("Compare files by checksum"),
           profile.transfer.checksum,
           [this](bool on) { profile_.transfer.checksum = on; },
           QStringLiteral("--checksum\nCompare whole-file checksums instead "
                          "of size and time."));
  addCheck(behaviours,
           QStringLiteral("Keep partial files in .rsync-partial"),
           !profile.transfer.partialDir.isEmpty(),
           [this](bool on) {
             profile_.transfer.partialDir =
                 on ? QStringLiteral(".rsync-partial") : QString();
           },
           QStringLiteral(
               "--partial-dir=.rsync-partial and --partial\nKeep interrupted "
               "transfers in a side folder so they can resume."));
  addCheck(behaviours, QStringLiteral("Update files in place"),
           profile.transfer.inplace,
           [this](bool on) { profile_.transfer.inplace = on; },
           QStringLiteral("--inplace\nWrite updated files directly instead "
                          "of replacing them."));
  addCheck(behaviours, QStringLiteral("Use fuzzy transfers"),
           profile.transfer.fuzzy,
           [this](bool on) { profile_.transfer.fuzzy = on; },
           QStringLiteral("--fuzzy\nUse a similar local file as the transfer "
                          "basis."));
  addCheck(behaviours, QStringLiteral("Use whole-file transfers"),
           profile.transfer.wholeFile,
           [this](bool on) { profile_.transfer.wholeFile = on; },
           QStringLiteral("--whole-file\nCopy whole files instead of using "
                          "delta transfer."));
  addCheck(behaviours, QStringLiteral("Copy symlinks as copies"),
           profile.transfer.copyLinks,
           [this](bool on) { profile_.transfer.copyLinks = on; },
           QStringLiteral("--copy-links\nCopy symlink targets as files or "
                          "folders."));
  addCheck(behaviours,
           QStringLiteral("Do not cross file-system boundaries"),
           profile.transfer.oneFileSystem,
           [this](bool on) { profile_.transfer.oneFileSystem = on; },
           QStringLiteral("--one-file-system\nDo not cross mounted "
                          "file-system boundaries."));
  addCheck(behaviours, QStringLiteral("Ignore modification times"),
           profile.transfer.ignoreTimes,
           [this](bool on) { profile_.transfer.ignoreTimes = on; },
           QStringLiteral("--ignore-times\nTransfer files even when size and "
                          "time match."));
  addCheck(behaviours, QStringLiteral("Transfer sparse files efficiently"),
           profile.transfer.sparse,
           [this](bool on) { profile_.transfer.sparse = on; },
           QStringLiteral("--sparse\nStore sequences of zeroes efficiently."));
  optionsLayout->addWidget(behavioursGroup);

  auto* conflictGroup = new QGroupBox(QStringLiteral("Conflict policy"),
                                      options);
  auto* conflictLayout = new QFormLayout(conflictGroup);
  auto* conflict = addCombo(
      conflictLayout, QStringLiteral("Policy"),
      {{QStringLiteral("Override"), static_cast<int>(core::ConflictPolicy::Override)},
       {QStringLiteral("Skip"), static_cast<int>(core::ConflictPolicy::Skip)},
       {QStringLiteral("Unselect"), static_cast<int>(core::ConflictPolicy::Unselect)}},
      static_cast<int>(profile.conflict));
  addTip(conflict, QStringLiteral("conflict policy"),
         QStringLiteral(
             "Controls what happens when a file changes on both sides between "
             "the comparison and the synchronization."));
  connect(conflict, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this, conflict](int index) {
            profile_.conflict = static_cast<core::ConflictPolicy>(
                conflict->itemData(index).toInt());
          });
  auto* conflictHint = new QLabel(
      QStringLiteral(
          "The conflict policy controls what happens when a file changes on "
          "both sides between the comparison and the synchronization."),
      conflictGroup);
  conflictHint->setWordWrap(true);
  conflictLayout->addRow(QString(), conflictHint);
  optionsLayout->addWidget(conflictGroup);

  auto* comparisonGroup = new QGroupBox(QStringLiteral("Comparison"), options);
  auto* comparisonLayout = new QVBoxLayout(comparisonGroup);
  addCheck(comparisonLayout,
           QStringLiteral(
               "Fetch the unchanged inventory automatically after a comparison"),
           profile.inventoryAfterCompare,
           [this](bool on) { profile_.inventoryAfterCompare = on; });
  optionsLayout->addWidget(comparisonGroup);
  optionsLayout->addStretch(1);
  tabs->addTab(options, QStringLiteral("Options"));

  auto* preservation = new QWidget(tabs);
  auto* preservationLayout = new QVBoxLayout(preservation);
  addCheck(preservationLayout, QStringLiteral("Archive mode"),
           profile.preservation.archive,
           [this](bool on) { profile_.preservation.archive = on; },
           QStringLiteral("--archive\nRecurse and preserve permissions, times, "
                          "links and devices."));
  addCheck(preservationLayout, QStringLiteral("Preserve hard links"),
           profile.preservation.hardLinks,
           [this](bool on) { profile_.preservation.hardLinks = on; },
           QStringLiteral("--hard-links\nPreserve hard links between files."));
  addCheck(preservationLayout, QStringLiteral("Preserve ACLs"),
           profile.preservation.acls,
           [this](bool on) { profile_.preservation.acls = on; },
           QStringLiteral("--acls\nPreserve POSIX access control lists."));
  addCheck(preservationLayout, QStringLiteral("Preserve extended attributes"),
           profile.preservation.xattrs,
           [this](bool on) { profile_.preservation.xattrs = on; },
           QStringLiteral("--xattrs\nPreserve extended attributes."));
  addCheck(preservationLayout, QStringLiteral("Keep numeric IDs"),
           profile.preservation.numericIds,
           [this](bool on) { profile_.preservation.numericIds = on; },
           QStringLiteral("--numeric-ids\nKeep numeric owner and group IDs."));
  preservationLayout->addStretch(1);
  tabs->addTab(preservation, QStringLiteral("Preservation"));

  auto* backup = new QWidget(tabs);
  auto* backupLayout = new QVBoxLayout(backup);
  auto* backupRow = new QHBoxLayout();
  auto* backupEdit = new QLineEdit(profile.backup.directory, backup);
  backupEdit->setPlaceholderText(QStringLiteral("Empty disables backups"));
  addTip(backupEdit, QStringLiteral("--backup and --backup-dir=DIRECTORY"),
         QStringLiteral(
             "Keep replaced files in a destination-side backup folder."));
  backupRow->addWidget(backupEdit, 1);
  auto* browse = new QToolButton(backup);
  browse->setIcon(icons::toolbarIcon(QStringLiteral("folder-add")));
  browse->setToolTip(QStringLiteral("Browse for the backup directory"));
  browse->setAccessibleName(QStringLiteral("Browse backup directory"));
  auto* open = new QToolButton(backup);
  open->setIcon(icons::toolbarIcon(QStringLiteral("folder")));
  open->setToolTip(QStringLiteral("Open the backup directory"));
  open->setAccessibleName(QStringLiteral("Open backup directory"));
  backupRow->addWidget(browse);
  backupRow->addWidget(open);
  backupLayout->addLayout(backupRow);
  addCheck(backupLayout, QStringLiteral("Protect nested backup directory"),
           profile.backup.protectNested,
           [this](bool on) { profile_.backup.protectNested = on; },
           QStringLiteral("P /.backup/***\nProtect the nested backup folder "
                          "from deletion."));
  auto* backupHint = new QLabel(
      QStringLiteral(
          "An empty directory disables backups. The same directory is used by "
          "the backup toolbar button and by the profile."),
      backup);
  backupHint->setWordWrap(true);
  backupLayout->addWidget(backupHint);
  backupLayout->addStretch(1);
  connect(backupEdit, &QLineEdit::textChanged, this,
          [this](const QString& text) { profile_.backup.directory = text; });
  connect(browse, &QToolButton::clicked, this, [this, backupEdit]() {
    const QString chosen = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose backup directory"),
        profile_.backup.directory);
    if (!chosen.isEmpty()) backupEdit->setText(chosen);
  });
  connect(open, &QToolButton::clicked, this, [this]() {
    if (!profile_.backup.directory.isEmpty()) {
      QDesktopServices::openUrl(
          QUrl::fromLocalFile(profile_.backup.directory));
    }
  });
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
  auto* preview = new QPlainTextEdit(filters);
  preview->setReadOnly(true);
  preview->setMaximumHeight(preview->fontMetrics().lineSpacing() * 2 + 12);
  preview->setPlaceholderText(QStringLiteral("Line preview"));
  filtersLayout->addWidget(preview);
  auto* filterButtons = new QHBoxLayout();
  auto* importButton = new QPushButton(QStringLiteral("Import"), filters);
  auto* exportButton = new QPushButton(QStringLiteral("Export"), filters);
  auto* helpButton = new QPushButton(QStringLiteral("Help"), filters);
  filterButtons->addWidget(importButton);
  filterButtons->addWidget(exportButton);
  filterButtons->addWidget(helpButton);
  filterButtons->addStretch(1);
  filtersLayout->addLayout(filterButtons);
  connect(filterEdit, &QPlainTextEdit::textChanged, this,
          [this, filterEdit]() {
            profile_.filterText = filterEdit->toPlainText();
            profile_.filters = core::filterTextToRules(profile_.filterText);
          });
  connect(importButton, &QPushButton::clicked, this,
          [this, filterEdit]() {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Import filters"));
            if (path.isEmpty()) return;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            filterEdit->setPlainText(QString::fromUtf8(file.readAll()));
          });
  connect(exportButton, &QPushButton::clicked, this,
          [this, filterEdit]() {
            const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Export filters"));
            if (path.isEmpty()) return;
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
            file.write(filterEdit->toPlainText().toUtf8());
          });
  connect(helpButton, &QPushButton::clicked, this, [this]() {
    QMessageBox::information(
        this, QStringLiteral("rsync filter rules"),
        QStringLiteral(
            "Each non-empty, non-comment line is one rule, applied in order; "
            "rsync stops at the first matching rule.\n\n"
            "Operators: + include, - exclude, P protect, R risk, H hide, "
            "S show, . merge, : dir-merge, ! clear. Lines without an operator "
            "are exclusions. A leading # comments a rule out.\n\n"
            "Patterns: * matches one path component, ** crosses slashes, ? "
            "matches one character, [..] is a character class. A trailing / "
            "matches directories only and a leading / anchors to the root."));
  });
  const auto updatePreview = [filterEdit, preview]() {
    const int number = filterEdit->textCursor().blockNumber() + 1;
    const QString line = filterEdit->textCursor().block().text().trimmed();
    QString text;
    if (line.isEmpty()) {
      text = QStringLiteral("Line %1: blank, ignored").arg(number);
    } else if (line.startsWith('#')) {
      text = QStringLiteral("Line %1: comment, ignored").arg(number);
    } else if (core::isAdvancedRule(line)) {
      text = QStringLiteral("Line %1: rsync rule: %2").arg(number).arg(line);
    } else {
      text = QStringLiteral("Line %1: exclusion: - %2").arg(number).arg(line);
    }
    if (!line.isEmpty() && !line.startsWith('#')) {
      core::FilterRule rule;
      rule.rule = line;
      rule.type = core::RuleType::Advanced;
      if (!core::validateRule(rule)) {
        text = QStringLiteral("Line %1: ERROR: invalid rsync rule").arg(number);
      }
    }
    preview->setPlainText(text);
    preview->setToolTip(text);
  };
  connect(filterEdit, &QPlainTextEdit::cursorPositionChanged, this,
          updatePreview);
  updatePreview();
  tabs->addTab(filters, QStringLiteral("Filters"));

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    for (const core::FilterRule& rule : profile_.filters) {
      if (!core::validateRule(rule)) {
        QMessageBox::warning(
            this, QStringLiteral("Invalid filters"),
            QStringLiteral("Correct the invalid line in the Filters tab first."));
        return;
      }
    }
    accept();
  });
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
