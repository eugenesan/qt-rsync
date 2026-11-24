#include "ui/MainWindow.h"

#include <QDialogButtonBox>
#include <QDebug>
#include <QFile>
#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QFormLayout>
#include <QMenu>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QTime>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QRegularExpression>
#include <QSplitter>
#include <QTemporaryFile>
#include <QComboBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/Command.h"
#include "core/Filter.h"
#include "core/Parser.h"
#include "core/Paths.h"
#include "core/Storage.h"
#include "ui/ComparisonView.h"
#include "ui/OverviewPane.h"
#include "ui/SettingsDialog.h"
#include "ui/SyncDialog.h"
#include "ui/LockedProfileDialog.h"
#include "ui/Icons.h"

namespace qt_rsync::ui {
namespace {

QString withTrailingSlash(QString path) {
  if (!path.trimmed().isEmpty() && !path.endsWith('/')) path += '/';
  return path;
}

void setProcessEnvironment(const core::ProcessSpec& spec, QProcess* process) {
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  for (auto it = spec.environment.cbegin(); it != spec.environment.cend(); ++it) {
    environment.insert(it.key(), it.value());
  }
  process->setProcessEnvironment(environment);
}

}  // namespace

class MainWindow::Private {
 public:
  QLineEdit* source = nullptr;
  QLineEdit* destination = nullptr;
  QToolButton* compare = nullptr;
  QToolButton* verify = nullptr;
  QToolButton* sync = nullptr;
  QToolButton* undo = nullptr;
  QToolButton* cancel = nullptr;
  QToolButton* preserve = nullptr;
  QToolButton* inventoryAfter = nullptr;
  QToolButton* filtersButton = nullptr;
  QToolButton* settingsButton = nullptr;
  QComboBox* mode = nullptr;
  QLineEdit* search = nullptr;
  bool cancelled = false;
  QLineEdit* status = nullptr;
  QLabel* progressLabel = nullptr;
  QPlainTextEdit* commandPreview = nullptr;
  ActionFilterBar* filterBar = nullptr;
  QTextEdit* journal = nullptr;
  QLineEdit* journalFilter = nullptr;
  QStringList journalLines;
  QProgressBar* progress = nullptr;
  ComparisonView* comparison = nullptr;
  OverviewPane* overview = nullptr;
  QProcess* compareProcess = nullptr;
  QProcess* operationProcess = nullptr;
  QProcess* inventoryProcess = nullptr;
  QProcess* destinationInventoryProcess = nullptr;
  QVector<core::Change> inventoryChanges;
  QTemporaryFile* filterFile = nullptr;
  core::SyncProfile profile;
  QVector<core::Change> changes;
  core::ExecutionPlan plan;
  bool verified = false;
  bool stale = false;
  bool syncing = false;
  bool verifying = false;
  QString operationOutput;
  bool startupAborted = false;
  bool tempProfile = false;
  QString profilePath;
  std::unique_ptr<core::storage::FileLock> profileLock;
  QToolButton* profileButton = nullptr;
  QString logPath;
  QFile* logFile = nullptr;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), d_(new Private) {
  setWindowTitle(QStringLiteral("qt-rsync"));
  resize(1280, 800);

  auto* root = new QWidget(this);
  setCentralWidget(root);
  auto* rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(3, 2, 3, 2);
  rootLayout->setSpacing(3);

  // Row 1: application toolbar.
  auto* toolbar = new QToolBar(QStringLiteral("Main"), this);
  addToolBar(toolbar);
  toolbar->setIconSize(QSize(24, 24));
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  d_->mode = new QComboBox(toolbar);
  d_->mode->addItem(QStringLiteral("Update"), static_cast<int>(core::SyncMode::Update));
  d_->mode->addItem(QStringLiteral("Mirror"), static_cast<int>(core::SyncMode::Mirror));
  d_->mode->addItem(QStringLiteral("New Only"), static_cast<int>(core::SyncMode::NewOnly));
  connect(d_->mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            d_->profile.transfer.mode =
                static_cast<core::SyncMode>(d_->mode->itemData(index).toInt());
          });
  toolbar->addWidget(d_->mode);
  const auto styleTool = [](QToolButton* button) {
    button->setIconSize(QSize(24, 24));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(40, 40);
  };
  const auto addTool = [&](QToolButton*& button, const QString& text,
                           const QString& iconName, const QString& tip) {
    button = new QToolButton(toolbar);
    button->setText(text);
    button->setIcon(icons::toolbarIcon(iconName));
    button->setIconSize(QSize(24, 24));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(40, 40);
    button->setToolTip(tip);
    toolbar->addWidget(button);
  };
  auto* swapButton = new QToolButton(toolbar);
  swapButton->setText(QStringLiteral("Swap"));
  swapButton->setIcon(icons::toolbarIcon(QStringLiteral("undo")));
  swapButton->setToolTip(QStringLiteral("Swap Source and Destination"));
  connect(swapButton, &QToolButton::clicked, this, [this]() {
    const QString source = d_->source->text();
    d_->source->setText(d_->destination->text());
    d_->destination->setText(source);
  });
  styleTool(swapButton);
  toolbar->addWidget(swapButton);
  addTool(d_->compare, QStringLiteral("Compare"), QStringLiteral("compare"),
          QStringLiteral("Compare Source and Destination (F5)"));
  addTool(d_->preserve, QStringLiteral("Preserve"), QStringLiteral("undo"),
          QStringLiteral("Preserve decisions"));
  d_->preserve->setCheckable(true);
  d_->preserve->setChecked(true);
  addTool(d_->inventoryAfter, QStringLiteral("Inventory"), QStringLiteral("folder"),
          QStringLiteral("Inventory after comparison"));
  d_->inventoryAfter->setCheckable(true);
  addTool(d_->verify, QStringLiteral("Verify"), QStringLiteral("verify"),
          QStringLiteral("Verify Decisions (F8)"));
  addTool(d_->sync, QStringLiteral("Synchronize"), QStringLiteral("sync"),
          QStringLiteral("Synchronize (F9)"));
  addTool(d_->cancel, QStringLiteral("Cancel"), QStringLiteral("cancel"),
          QStringLiteral("Cancel the running operation (F10)"));
  auto* pauseButton = new QToolButton(toolbar);
  pauseButton->setText(QStringLiteral("Pause"));
  pauseButton->setIcon(icons::toolbarIcon(QStringLiteral("cancel")));
  pauseButton->setCheckable(true);
  pauseButton->setToolTip(QStringLiteral("Pause the running operation (F11)"));
  connect(pauseButton, &QToolButton::toggled, this, [this](bool checked) {
    if (checked) appendJournalLine(QStringLiteral("Paused"));
  });
  styleTool(pauseButton);
  toolbar->addWidget(pauseButton);
  toolbar->addSeparator();
  addTool(d_->filtersButton, QStringLiteral("Filters"), QStringLiteral("settings"),
          QStringLiteral("Edit the persistent filter rules (F7)"));

  d_->profileButton = new QToolButton(toolbar);
  d_->profileButton->setText(QStringLiteral("Profile"));
  d_->profileButton->setIcon(icons::toolbarIcon(QStringLiteral("settings")));
  d_->profileButton->setMenu(new QMenu(d_->profileButton));
  d_->profileButton->setPopupMode(QToolButton::InstantPopup);
  connect(d_->profileButton->menu(), &QMenu::aboutToShow,
          this, &MainWindow::updateProfileMenu);
  styleTool(d_->profileButton);
  toolbar->addWidget(d_->profileButton);
  addTool(d_->settingsButton, QStringLiteral("Settings"), QStringLiteral("settings"),
          QStringLiteral("Settings"));
  connect(d_->settingsButton, &QAbstractButton::clicked, this, [this]() {
    SettingsDialog dialog(this);
    dialog.exec();
  });
  connect(d_->filtersButton, &QAbstractButton::clicked, this, [this]() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Filters"));
    dialog.resize(640, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* editor = new QPlainTextEdit(&dialog);
    editor->setPlainText(d_->profile.filterText);
    layout->addWidget(editor);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
      d_->profile.filterText = editor->toPlainText();
      d_->profile.filters = core::filterTextToRules(d_->profile.filterText);
    }
  });
  auto* layoutButton = new QToolButton(toolbar);
  layoutButton->setText(QStringLiteral("Layout"));
  layoutButton->setIcon(icons::toolbarIcon(QStringLiteral("settings")));
  auto* layoutMenu = new QMenu(layoutButton);
  const QVector<QPair<QString, QString>> layoutModes{
      {QStringLiteral("Normal"), QStringLiteral("normal")},
      {QStringLiteral("Compare only"), QStringLiteral("focus")},
      {QStringLiteral("Compare with overview"), QStringLiteral("wide")},
  };
  for (const auto& mode : layoutModes) {
    auto* action = layoutMenu->addAction(mode.first);
    action->setCheckable(true);
    action->setChecked(mode.second == QStringLiteral("normal"));
    connect(action, &QAction::triggered, layoutButton, [this, layoutMenu, action]() {
      for (QAction* other : layoutMenu->actions()) other->setChecked(other == action);
      d_->overview->setVisible(true);
    });
  }
  layoutButton->setMenu(layoutMenu);
  layoutButton->setPopupMode(QToolButton::InstantPopup);
  styleTool(layoutButton);
  toolbar->addWidget(layoutButton);
  auto* backupButton = new QToolButton(toolbar);
  backupButton->setText(QStringLiteral("Backup"));
  backupButton->setIcon(icons::folderIcon());
  backupButton->setToolTip(QStringLiteral("Open the backup directory"));
  connect(backupButton, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->profile.backup.directory));
  });
  styleTool(backupButton);
  toolbar->addWidget(backupButton);
  auto* logButton = new QToolButton(toolbar);
  logButton->setText(QStringLiteral("Log"));
  logButton->setIcon(icons::toolbarIcon(QStringLiteral("verify")));
  logButton->setToolTip(QStringLiteral("Open the log file"));
  connect(logButton, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->logPath));
  });
  styleTool(logButton);
  toolbar->addWidget(logButton);
  toolbar->addSeparator();
  toolbar->addWidget(new QLabel(QStringLiteral("Search"), toolbar));
  d_->search = new QLineEdit(toolbar);
  d_->search->setObjectName(QStringLiteral("search-edit"));
  d_->search->setPlaceholderText(QStringLiteral("Search the list..."));
  d_->search->setClearButtonEnabled(true);
  d_->search->setFixedWidth(220);
  toolbar->addWidget(d_->search);

  // Row 2: source + action filter bar + destination.
  auto* locations = new QHBoxLayout();
  auto* sourcePanel = new QWidget(root);
  auto* sourceLayout = new QHBoxLayout(sourcePanel);
  sourceLayout->setContentsMargins(0, 0, 0, 0);
  sourceLayout->setSpacing(3);
  d_->source = new QLineEdit(sourcePanel);
  d_->source->setObjectName(QStringLiteral("source-edit"));
  d_->source->setPlaceholderText(QStringLiteral("Local: /path or ~/path    SSH: [user@]host:/absolute/path"));
  sourceLayout->addWidget(d_->source, 1);
  auto* sourceOpen = new QToolButton(sourcePanel);
  sourceOpen->setText(QStringLiteral("Open"));
  sourceOpen->setIcon(icons::toolbarIcon(QStringLiteral("compare")));
  sourceOpen->setToolTip(QStringLiteral("Open the Source directory"));
  connect(sourceOpen, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->source->text()));
  });
  sourceLayout->addWidget(sourceOpen);
  auto* sourceBrowse = new QToolButton(sourcePanel);
  sourceBrowse->setText(QStringLiteral("..."));
  sourceBrowse->setIcon(icons::folderIcon());
  connect(sourceBrowse, &QToolButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Browse to Source location"));
    if (!dir.isEmpty()) d_->source->setText(dir);
  });
  sourceLayout->addWidget(sourceBrowse);

  auto* filterColumn = new QWidget(root);
  auto* filterLayout = new QVBoxLayout(filterColumn);
  filterLayout->setContentsMargins(0, 0, 0, 0);
  filterLayout->setAlignment(Qt::AlignBottom);
  d_->filterBar = new ActionFilterBar(filterColumn);
  filterLayout->addWidget(d_->filterBar);

  auto* destinationPanel = new QWidget(root);
  auto* destinationLayout = new QHBoxLayout(destinationPanel);
  destinationLayout->setContentsMargins(0, 0, 0, 0);
  destinationLayout->setSpacing(3);
  d_->destination = new QLineEdit(destinationPanel);
  d_->destination->setObjectName(QStringLiteral("destination-edit"));
  d_->destination->setPlaceholderText(d_->source->placeholderText());
  destinationLayout->addWidget(d_->destination, 1);
  auto* destinationOpen = new QToolButton(destinationPanel);
  destinationOpen->setText(QStringLiteral("Open"));
  destinationOpen->setIcon(icons::toolbarIcon(QStringLiteral("compare")));
  destinationOpen->setToolTip(QStringLiteral("Open the Destination directory"));
  connect(destinationOpen, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->destination->text()));
  });
  destinationLayout->addWidget(destinationOpen);
  auto* destinationBrowse = new QToolButton(destinationPanel);
  destinationBrowse->setText(QStringLiteral("..."));
  destinationBrowse->setIcon(icons::folderIcon());
  connect(destinationBrowse, &QToolButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Browse to Destination location"));
    if (!dir.isEmpty()) d_->destination->setText(dir);
  });
  destinationLayout->addWidget(destinationBrowse);

  locations->addWidget(sourcePanel, 40);
  locations->addWidget(filterColumn, 0, Qt::AlignBottom);
  locations->addWidget(destinationPanel, 40);
  rootLayout->addLayout(locations);

  // Row 3: comparison above, journal + overview below.
  auto* splitter = new QSplitter(Qt::Vertical, root);
  d_->comparison = new ComparisonView(splitter);
  d_->comparison->setFilterBar(d_->filterBar);
  splitter->addWidget(d_->comparison);

  auto* logPanel = new QWidget(splitter);
  auto* logPanelLayout = new QHBoxLayout(logPanel);
  logPanelLayout->setContentsMargins(0, 0, 0, 0);
  logPanelLayout->setSpacing(4);
  auto* logSplitter = new QSplitter(Qt::Horizontal, logPanel);
  logSplitter->setChildrenCollapsible(false);
  auto* logColumn = new QWidget(logSplitter);
  auto* logColumnLayout = new QVBoxLayout(logColumn);
  logColumnLayout->setContentsMargins(0, 0, 0, 0);
  logColumnLayout->setSpacing(2);

  d_->progress = new QProgressBar(logColumn);
  d_->progress->setRange(0, 100);
  d_->progress->setValue(0);
  d_->progress->setFormat(QStringLiteral("%p%"));
  d_->progress->hide();
  logColumnLayout->addWidget(d_->progress);
  d_->progressLabel = new QLabel(logColumn);
  logColumnLayout->addWidget(d_->progressLabel);

  d_->commandPreview = new QPlainTextEdit(logColumn);
  d_->commandPreview->setObjectName(QStringLiteral("command-preview-label"));
  d_->commandPreview->setReadOnly(true);
  d_->commandPreview->setPlaceholderText(QStringLiteral("Command preview appears here."));
  d_->commandPreview->setMaximumHeight(64);
  logColumnLayout->addWidget(d_->commandPreview);

  auto* journalBar = new QWidget(logColumn);
  auto* journalBarLayout = new QHBoxLayout(journalBar);
  journalBarLayout->setContentsMargins(0, 0, 0, 0);
  journalBarLayout->addWidget(new QLabel(QStringLiteral("Journal filter:"), journalBar));
  d_->journalFilter = new QLineEdit(journalBar);
  d_->journalFilter->setPlaceholderText(QStringLiteral("Filter journal lines..."));
  d_->journalFilter->setClearButtonEnabled(true);
  journalBarLayout->addWidget(d_->journalFilter, 1);
  connect(d_->journalFilter, &QLineEdit::textChanged, this, [this]() {
    d_->journal->clear();
    for (const QString& line : d_->journalLines) {
      if (d_->journalFilter->text().isEmpty() ||
          line.contains(d_->journalFilter->text(), Qt::CaseInsensitive)) {
        appendJournalLine(line);
      }
    }
  });
  logColumnLayout->addWidget(journalBar);

  d_->journal = new QTextEdit(logColumn);
  d_->journal->setReadOnly(true);
  logColumnLayout->addWidget(d_->journal, 1);
  logSplitter->addWidget(logColumn);
  d_->overview = new OverviewPane(logSplitter);
  logSplitter->setStretchFactor(0, 1);
  logSplitter->setStretchFactor(1, 0);
  logSplitter->setSizes({680, 620});
  logPanelLayout->addWidget(logSplitter);
  splitter->addWidget(logPanel);
  splitter->setSizes({500, 280});
  rootLayout->addWidget(splitter, 1);

  // Status bar.
  auto* statusBar = new QWidget(root);
  auto* statusLayout = new QHBoxLayout(statusBar);
  statusLayout->setContentsMargins(0, 0, 0, 0);
  statusLayout->addWidget(new QLabel(QStringLiteral("State:"), statusBar));
  d_->status = new QLineEdit(statusBar);
  d_->status->setObjectName(QStringLiteral("status-label"));
  d_->status->setReadOnly(true);
  d_->status->setFrame(false);
  statusLayout->addWidget(d_->status, 1);
  rootLayout->addWidget(statusBar);

  d_->compareProcess = new QProcess(this);
  d_->operationProcess = new QProcess(this);
  d_->inventoryProcess = new QProcess(this);
  d_->destinationInventoryProcess = new QProcess(this);
  connect(d_->compare, &QAbstractButton::clicked, this, &MainWindow::startCompare);
  connect(d_->verify, &QAbstractButton::clicked, this, &MainWindow::verifyPlan);
  connect(d_->sync, &QAbstractButton::clicked, this, &MainWindow::synchronize);
  connect(d_->cancel, &QAbstractButton::clicked, this, &MainWindow::cancelOperation);
  connect(d_->preserve, &QAbstractButton::clicked, this, [this]() {
    core::storage::Settings settings;
    settings.setValue(QStringLiteral("decisions/preserve"),
                      d_->preserve->isChecked());
    settings.sync();
  });
  auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
  connect(undoShortcut, &QShortcut::activated, this,
          [this]() { d_->comparison->undoDecision(); });
  const auto addShortcut = [this](int key, auto slot) {
    auto* shortcut = new QShortcut(QKeySequence(key), this);
    connect(shortcut, &QShortcut::activated, this, slot);
  };
  addShortcut(Qt::Key_F5, &MainWindow::startCompare);
  addShortcut(Qt::Key_F8, &MainWindow::verifyPlan);
  addShortcut(Qt::Key_F9, &MainWindow::synchronize);
  addShortcut(Qt::Key_F10, &MainWindow::cancelOperation);
  connect(d_->filterBar, &ActionFilterBar::unchangedToggled, this,
          [this](bool enabled) {
            if (enabled) startInventory();
          });
  connect(d_->filterBar, &ActionFilterBar::undoRequested, this,
          [this]() { d_->comparison->undoDecision(); });
  connect(d_->comparison, &ComparisonView::undoAvailable, d_->filterBar,
          &ActionFilterBar::setUndoEnabled);
  connect(d_->compareProcess, &QProcess::finished, this, &MainWindow::onCompareFinished);
  connect(d_->compareProcess, &QProcess::errorOccurred, this, &MainWindow::onCompareError);
  connect(d_->operationProcess, &QProcess::finished, this, &MainWindow::onOperationFinished);
  connect(d_->operationProcess, &QProcess::errorOccurred, this, &MainWindow::onOperationError);
  connect(d_->inventoryProcess, &QProcess::finished, this, &MainWindow::onInventoryFinished);
  connect(d_->destinationInventoryProcess, &QProcess::finished, this,
          &MainWindow::onDestinationInventoryFinished);
  connect(d_->operationProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    const QString text = core::cleanQtNoise(
        QString::fromLocal8Bit(d_->operationProcess->readAllStandardOutput()));
    d_->operationOutput += text;
    for (const QString& line : text.split('\r')) {
      const QString clean = line.trimmed();
      if (clean.isEmpty()) continue;
      static const QRegularExpression percent(QStringLiteral("(\\d+)%"));
      const auto match = percent.match(clean);
      if (match.hasMatch()) d_->progress->setValue(match.captured(1).toInt());
      appendJournalLine(clean);
    }
  });
  restoreProfile();

  connect(d_->comparison, &ComparisonView::decisionChanged, this, [this]() {
    d_->verified = false;
    refreshPlan();
    d_->filterBar->setExcludedCount(
        d_->comparison->decisions().excludedPaths().size());
    d_->overview->rebuild(d_->comparison->store());
  });
  connect(d_->comparison, &ComparisonView::filterRequested, this,
          [this](const QString& pattern, const QString& path) {
            addFilterPattern(pattern, path);
          });
  connect(d_->comparison, &ComparisonView::deleteFilterRequested, this,
          [this](const QString& pattern, const QString& path) {
            addFilterPattern(pattern, path);
          });
  connect(d_->comparison, &ComparisonView::deleteRequested, this,
          [this](const QString& path) { d_->comparison->markDelete(path); });
  connect(d_->comparison, &ComparisonView::diffRequested, this, [this](const QString& path) {
    appendJournalLine(QStringLiteral("Diff tool requested for ") + path);
  });
  connect(d_->comparison, &ComparisonView::openDirectoryRequested, this,
          [this](int pane, const QString&) {
    const QString root = pane == 2 ? d_->profile.destination.path
                                   : d_->profile.source.path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(root));
  });
}

MainWindow::~MainWindow() {
  discardTempProfile();
  closeSessionLog();
  delete d_;
}

void MainWindow::applyStartupOptions(const app::StartupOptions& options) {
  // Empty options mean no --dir-pair was given; keep the profile's endpoints.
  if (!options.source.isEmpty()) {
    d_->source->setText(withTrailingSlash(options.source));
  }
  if (!options.destination.isEmpty()) {
    d_->destination->setText(withTrailingSlash(options.destination));
  }
  d_->profile.backup.directory = options.backup;
  if (options.mode == QLatin1String("mirror")) {
    d_->profile.transfer.mode = core::SyncMode::Mirror;
  } else if (options.mode == QLatin1String("new-only")) {
    d_->profile.transfer.mode = core::SyncMode::NewOnly;
  }
  if (options.compare && !options.source.isEmpty() && !options.destination.isEmpty()) {
    startCompare();
  }
}

void MainWindow::appendJournalLine(const QString& line) {
  QString colour = QStringLiteral("#9CA3AF");
  if (line.contains(QStringLiteral("fail"), Qt::CaseInsensitive) ||
      line.contains(QStringLiteral("error"), Qt::CaseInsensitive) ||
      line.contains(QStringLiteral("cancel"), Qt::CaseInsensitive) ||
      line.contains(QStringLiteral("mismatch"), Qt::CaseInsensitive)) {
    colour = QStringLiteral("#DC2626");
  } else if (line.contains(QStringLiteral("complete"), Qt::CaseInsensitive) ||
             line.contains(QStringLiteral("matched"), Qt::CaseInsensitive)) {
    colour = QStringLiteral("#15803D");
  } else if (line.contains(QStringLiteral("warning"), Qt::CaseInsensitive)) {
    colour = QStringLiteral("#B45309");
  }
  d_->journal->append(QStringLiteral("<span style='color:%1'>%2</span>")
                          .arg(colour, line.toHtmlEscaped()));
}

void MainWindow::setCommandPreview(const core::ProcessSpec& spec) {
  d_->commandPreview->setPlainText(QStringLiteral("$ ") + spec.argv().join(' '));
  d_->commandPreview->setToolTip(d_->commandPreview->toPlainText());
}

void MainWindow::setStatus(const QString& text) {
  d_->status->setText(text);
  const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
  const QString line = time + QStringLiteral(" ") + text;
  d_->journalLines.append(line);
  appendJournalLine(line);
  if (d_->logFile != nullptr && d_->logFile->isOpen()) {
    d_->logFile->write((line + '\n').toUtf8());
    d_->logFile->flush();
  }
}

void MainWindow::startCompare() {
  d_->source->setText(withTrailingSlash(d_->source->text()));
  d_->destination->setText(withTrailingSlash(d_->destination->text()));
  const auto endpointFromField = [](QLineEdit* field,
                                    const core::Endpoint& current) {
    const QString text = field->text();
    // A profile-loaded SSH endpoint must survive as SSH even though the field
    // only shows its operand; only reparse when the user actually edits it.
    if (!text.isEmpty() &&
        (text == current.rsyncOperand() || text == current.path)) {
      return current;
    }
    return core::parseEndpoint(text);
  };
  d_->profile.source = endpointFromField(d_->source, d_->profile.source);
  d_->profile.destination =
      endpointFromField(d_->destination, d_->profile.destination);
  if (d_->profile.source.path.isEmpty() || d_->profile.destination.path.isEmpty()) {
    setStatus(QStringLiteral("Enter Source and Destination first."));
    return;
  }
  d_->profile.filters = core::filterTextToRules(d_->profile.filterText);
  const core::ProcessSpec spec = core::buildCompareSpec(d_->profile);
  setProcessEnvironment(spec, d_->compareProcess);
  setCommandPreview(spec);
  d_->compare->setEnabled(false);
  d_->cancel->setEnabled(true);
  setStatus(QStringLiteral("Comparing..."));
  d_->compareProcess->start(spec.program, spec.arguments);
}

void MainWindow::onCompareFinished(int exitCode, QProcess::ExitStatus status) {
  d_->compare->setEnabled(true);
  d_->cancel->setEnabled(false);
  if (d_->cancelled) {
    d_->cancelled = false;
    setStatus(QStringLiteral("Comparison cancelled"));
    return;
  }
  const QByteArray output = d_->compareProcess->readAllStandardOutput();
  const QString stderr = QString::fromLocal8Bit(d_->compareProcess->readAllStandardError());
  if (status != QProcess::NormalExit || exitCode != 0) {
    setStatus(QStringLiteral("Compare failed: %1")
                            .arg(stderr.trimmed().isEmpty()
                                     ? QString::number(exitCode)
                                     : stderr.trimmed()));
    return;
  }
  const core::ParseResult parsed = core::parseOutput(
      QString::fromUtf8(output), d_->profile.transfer.mirrorEnabled());
  d_->changes = parsed.changes;
  const auto baseName = [](const QString& operand) {
    QString clean = operand.trimmed();
    while (clean.endsWith('/')) clean.chop(1);
    const QString name = clean.section('/', -1);
    return name.isEmpty() ? QStringLiteral("?") : name;
  };
  d_->comparison->setRootNames(baseName(d_->profile.source.path),
                               baseName(d_->profile.destination.path));
  d_->comparison->setChanges(parsed.changes);
  d_->overview->rebuild(d_->comparison->store());
  refreshPlan();
  d_->stale = false;
  startDestinationInventory();
  setStatus(QStringLiteral("Comparison complete: %1 item(s)")
                          .arg(parsed.changes.size()));
}

void MainWindow::onCompareError(QProcess::ProcessError error) {
  Q_UNUSED(error)
  d_->compare->setEnabled(true);
  d_->cancel->setEnabled(false);
  setStatus(QStringLiteral("Could not start rsync: %1")
                          .arg(d_->compareProcess->errorString()));
}

void MainWindow::startDestinationInventory() {
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.destination);
  setProcessEnvironment(spec, d_->inventoryProcess);
  d_->inventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::startInventory() {
  d_->inventoryChanges.clear();
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.source);
  setProcessEnvironment(spec, d_->inventoryProcess);
  d_->inventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::onInventoryFinished(int, QProcess::ExitStatus) {
  if (d_->cancelled) return;
  const QString text = core::cleanQtNoise(
      QString::fromLocal8Bit(d_->inventoryProcess->readAllStandardOutput()));
  QHash<QString, QPair<qlonglong, QString>> metadata;
  static const QRegularExpression line(
      QStringLiteral("^\\S+\\s+(\\d+)\\s+(\\d{4}/\\d{2}/\\d{2})\\s+(\\d{2}:\\d{2}:\\d{2})\\s+(.+)$"));
  for (const QString& entry : text.split('\n')) {
    const auto match = line.match(entry.trimmed());
    if (!match.hasMatch()) continue;
    metadata.insert(match.captured(4),
                    {match.captured(1).toLongLong(),
                     match.captured(2) + ' ' + match.captured(3)});
  }
  if (!metadata.isEmpty()) d_->comparison->applyDestinationMetadata(metadata);
  const QString sourceText = core::cleanQtNoise(
      QString::fromLocal8Bit(d_->inventoryProcess->readAllStandardOutput()));
  d_->inventoryChanges = core::parseInventory(sourceText);
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.destination);
  setProcessEnvironment(spec, d_->destinationInventoryProcess);
  d_->destinationInventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::onDestinationInventoryFinished(int, QProcess::ExitStatus) {
  if (d_->cancelled) return;
  const QString text = core::cleanQtNoise(
      QString::fromLocal8Bit(d_->destinationInventoryProcess->readAllStandardOutput()));
  d_->inventoryChanges.append(core::parseInventory(text));
  if (!d_->inventoryChanges.isEmpty()) {
    QVector<core::Change> merged = d_->changes;
    for (const core::Change& change : d_->inventoryChanges) {
      bool present = false;
      for (const core::Change& existing : merged) {
        if (existing.path == change.path) { present = true; break; }
      }
      if (!present) merged.append(change);
    }
    d_->changes = merged;
    d_->comparison->setChanges(merged);
    d_->overview->rebuild(d_->comparison->store());
  }
}

void MainWindow::refreshPlan() {
  d_->plan = core::buildExecutionPlan(d_->profile, d_->changes,
                                      d_->comparison->decisions());
  const bool actionable = d_->plan.summary.included > 0;
  d_->verify->setEnabled(actionable);
  d_->sync->setEnabled(actionable);
}

void MainWindow::verifyPlan() {
  if (d_->plan.items.isEmpty()) refreshPlan();
  d_->filterFile = new QTemporaryFile(this);
  if (!d_->filterFile->open()) {
    setStatus(QStringLiteral("Could not write decision rules."));
    return;
  }
  d_->filterFile->write(core::decisionFilterText(d_->plan).toUtf8());
  d_->filterFile->flush();
  const core::ProcessSpec spec =
      core::buildVerifySpec(d_->profile, d_->filterFile->fileName());
  setProcessEnvironment(spec, d_->operationProcess);
  setCommandPreview(spec);
  d_->syncing = false;
  d_->verifying = true;
  d_->operationOutput.clear();
  d_->cancel->setEnabled(true);
  setStatus(QStringLiteral("Verifying..."));
  d_->operationProcess->start(spec.program, spec.arguments);
}

void MainWindow::synchronize() {
  if (d_->plan.items.isEmpty() || d_->stale) refreshPlan();
  d_->filterFile = new QTemporaryFile(this);
  if (!d_->filterFile->open()) {
    setStatus(QStringLiteral("Could not write decision rules."));
    return;
  }
  d_->filterFile->write(core::decisionFilterText(d_->plan).toUtf8());
  d_->filterFile->flush();

  SyncDialog dialog(d_->profile, d_->plan, d_->filterFile->fileName(), this);
  if (dialog.exec() != QDialog::Accepted) return;

  const core::ProcessSpec spec =
      core::buildSyncSpec(d_->profile, d_->filterFile->fileName());
  setProcessEnvironment(spec, d_->operationProcess);
  setCommandPreview(spec);
  d_->syncing = true;
  d_->verifying = false;
  d_->operationOutput.clear();
  d_->cancel->setEnabled(true);
  d_->progress->show();
  d_->progress->setValue(0);
  setStatus(QStringLiteral("Synchronizing..."));
  d_->operationProcess->start(spec.program, spec.arguments);
}

void MainWindow::onOperationFinished(int exitCode, QProcess::ExitStatus status) {
  d_->cancel->setEnabled(false);
  if (d_->cancelled) {
    d_->cancelled = false;
    d_->syncing = false;
    d_->verifying = false;
    d_->progress->hide();
    setStatus(QStringLiteral("Operation cancelled"));
    return;
  }
  d_->progress->hide();
  const QString stderr = QString::fromLocal8Bit(d_->operationProcess->readAllStandardError());
  const bool ok = status == QProcess::NormalExit && exitCode == 0;
  if (d_->syncing) {
    d_->syncing = false;
    if (!ok) {
      setStatus(QStringLiteral("Synchronization failed: %1")
                              .arg(stderr.trimmed()));
      return;
    }
    // The destination changed: the comparison is stale until the user asks
    // for a fresh Compare. Never refresh automatically.
    d_->stale = true;
    d_->verified = false;
    setStatus(QStringLiteral(
        "Synchronization complete — click Compare to refresh the comparison."));
    return;
  }
  d_->verifying = false;
  if (!ok) {
    d_->verified = false;
    setStatus(QStringLiteral("Verification failed: %1").arg(stderr.trimmed()));
    return;
  }
  const core::ParseResult parsed = core::parseOutput(
      d_->operationOutput, d_->profile.transfer.mirrorEnabled());
  const core::PlanMismatch mismatch = core::comparePlanToChanges(
      d_->plan, parsed.changes, &d_->comparison->decisions());
  d_->verified = mismatch.valid();
  if (d_->verified) {
    setStatus(QStringLiteral("Verification matched the plan."));
  } else {
    setStatus(QStringLiteral("Verification mismatch: %1 missing, %2 unexpected")
                  .arg(mismatch.missing.size())
                  .arg(mismatch.unexpected.size()));
    for (const QString& line : mismatch.missing) {
      appendJournalLine(QStringLiteral("missing: ") + line);
    }
    for (const QString& line : mismatch.unexpected) {
      appendJournalLine(QStringLiteral("unexpected: ") + line);
    }
  }
}

void MainWindow::onOperationError(QProcess::ProcessError error) {
  Q_UNUSED(error)
  d_->cancel->setEnabled(false);
  setStatus(QStringLiteral("Operation failed: %1")
                          .arg(d_->operationProcess->errorString()));
}

void MainWindow::cancelOperation() {
  QProcess* target = nullptr;
  if (d_->compareProcess->state() != QProcess::NotRunning) {
    target = d_->compareProcess;
  } else if (d_->operationProcess->state() != QProcess::NotRunning) {
    target = d_->operationProcess;
  } else if (d_->inventoryProcess->state() != QProcess::NotRunning) {
    target = d_->inventoryProcess;
  }
  if (target == nullptr) return;
  d_->cancelled = true;
  d_->cancel->setEnabled(false);
  setStatus(QStringLiteral("Cancelling..."));
  target->kill();
}

void MainWindow::addFilterPattern(const QString& pattern, const QString& path) {
  const QString text = d_->profile.filterText;
  if (text.split('\n').contains(pattern)) return;
  d_->profile.filterText =
      text.isEmpty() ? pattern
                     : text + QStringLiteral("\n# Added from ") + path +
                           QStringLiteral("\n") + pattern;
  d_->profile.filters = core::filterTextToRules(d_->profile.filterText);
  setStatus(QStringLiteral("Filter added: %1").arg(pattern));
}

bool MainWindow::startupAborted() const {
  return d_->startupAborted;
}

void MainWindow::closeSessionLog() {
  if (d_->logFile != nullptr) {
    d_->logFile->flush();
    delete d_->logFile;
    d_->logFile = nullptr;
  }
  d_->logPath.clear();
}

void MainWindow::openSessionLog(const QString& name) {
  closeSessionLog();
  d_->logPath = core::paths::appLogPath(name);
  QFileInfo(d_->logPath).absoluteDir().mkpath(QStringLiteral("."));
  d_->logFile = new QFile(d_->logPath, this);
  if (d_->logFile->open(QIODevice::Append | QIODevice::Text)) {
    d_->logFile->write(QStringLiteral("\n===== qt-rsync session =====\n").toUtf8());
    d_->logFile->flush();
  }
}

void MainWindow::promoteTempProfile(const QString& newName) {
  if (!d_->tempProfile) return;
  const QString oldName = QFileInfo(d_->profilePath).completeBaseName();
  closeSessionLog();
  const auto rename = [](const QString& from, const QString& to) {
    if (from != to && QFile::exists(from)) {
      QFile::remove(to);
      QFile::rename(from, to);
    }
  };
  rename(core::paths::appLogPath(oldName), core::paths::appLogPath(newName));
  rename(core::paths::rsyncLogPath(oldName), core::paths::rsyncLogPath(newName));
  QFile::remove(d_->profilePath);
  QFile::remove(d_->profilePath + QStringLiteral(".lock"));
  d_->profileLock.reset();
  d_->tempProfile = false;
  openSessionLog(newName);
}

void MainWindow::discardTempProfile() {
  if (!d_->tempProfile) return;
  const QString name = QFileInfo(d_->profilePath).completeBaseName();
  closeSessionLog();
  QFile::remove(core::paths::appLogPath(name));
  QFile::remove(core::paths::rsyncLogPath(name));
  QFile::remove(d_->profilePath);
  QFile::remove(d_->profilePath + QStringLiteral(".lock"));
  d_->profileLock.reset();
  d_->profilePath.clear();
  d_->tempProfile = false;
}

void MainWindow::setProfilePath(const QString& path) {
  d_->profilePath = path;
  if (!d_->tempProfile) {
    core::storage::setCurrentProfilePath(path);
    openSessionLog(QFileInfo(path).completeBaseName());
  }
  d_->profileButton->setText(QFileInfo(path).completeBaseName());
}

void MainWindow::loadProfileFile(const QString& path) {
  d_->profile = core::storage::loadProfile(path);
  d_->source->setText(d_->profile.source.rsyncOperand());
  d_->destination->setText(d_->profile.destination.rsyncOperand());
  setProfilePath(path);
  setStatus(QStringLiteral("Profile loaded: %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::restoreProfile() {
  const QString current = core::storage::currentProfilePath();
  if (current.isEmpty() || !QFile::exists(current)) {
    setStatus(QStringLiteral("No profile loaded"));
    return;
  }
  auto lock = std::make_unique<core::storage::FileLock>(current, 0);
  if (lock->acquire()) {
    d_->profileLock = std::move(lock);
    loadProfileFile(current);
    return;
  }
  const QString suggested = QFileInfo(current).completeBaseName() +
                            QStringLiteral("~tmp");
  auto result = promptLockedProfile(this, current, suggested);
  switch (result.choice) {
    case LockedChoice::Quit:
      d_->startupAborted = true;
      return;
    case LockedChoice::UseOriginal:
      d_->profileLock = std::move(result.lock);
      loadProfileFile(current);
      return;
    case LockedChoice::Temporary: {
      QString name = result.name;
      if (name.isEmpty()) name = suggested;
      if (!name.endsWith(QLatin1String(".json"))) name += QLatin1String(".json");
      const QString path = core::paths::profilesDirectory() + '/' + name;
      auto tempLock = std::make_unique<core::storage::FileLock>(path, 0);
      if (!tempLock->acquire()) {
        setStatus(QStringLiteral("Temporary profile name is locked."));
        return;
      }
      d_->profile = core::storage::loadProfile(current);
      core::storage::saveProfile(path, d_->profile);
      d_->profileLock = std::move(tempLock);
      d_->tempProfile = true;
  d_->source->setText(d_->profile.source.rsyncOperand());
  d_->destination->setText(d_->profile.destination.rsyncOperand());
      d_->profilePath = path;
      d_->profileButton->setText(QFileInfo(path).completeBaseName() +
                                 QStringLiteral(" (temporary)"));
      openSessionLog(QFileInfo(path).completeBaseName());
      setStatus(QStringLiteral("Using temporary profile: %1")
                    .arg(QFileInfo(path).fileName()));
      return;
    }
  }
}

void MainWindow::saveProfileAs() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save Profile"),
      core::paths::profilesDirectory() + QStringLiteral("/profile.json"),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) return;
  d_->profileLock.reset();
  auto lock = std::make_unique<core::storage::FileLock>(path, 0);
  if (!lock->acquire()) {
    setStatus(QStringLiteral("That profile is open in another instance."));
    return;
  }
  promoteTempProfile(QFileInfo(path).completeBaseName());
  d_->tempProfile = false;
  core::storage::saveProfile(path, d_->profile);
  d_->profileLock = std::move(lock);
  setProfilePath(path);
  setStatus(QStringLiteral("Profile saved: %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::saveCurrentProfile() {
  if (d_->profilePath.isEmpty()) {
    saveProfileAs();
    return;
  }
  core::storage::saveProfile(d_->profilePath, d_->profile);
  setProfilePath(d_->profilePath);
  setStatus(QStringLiteral("Profile saved"));
}

void MainWindow::loadProfile() {
  discardTempProfile();
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Load Profile"), core::paths::profilesDirectory(),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) return;
  auto lock = std::make_unique<core::storage::FileLock>(path, 0);
  if (!lock->acquire()) {
    auto result = promptLockedProfile(this, path,
                                            QFileInfo(path).completeBaseName() +
                                                QStringLiteral("~tmp"));
    if (result.choice == LockedChoice::Quit) return;
    if (result.choice == LockedChoice::UseOriginal) {
      d_->profileLock = std::move(result.lock);
      d_->tempProfile = false;
      loadProfileFile(path);
      return;
    }
    const QString tempPath = core::paths::profilesDirectory() + '/' +
                             result.name + QStringLiteral(".json");
    d_->profile = core::storage::loadProfile(path);
    core::storage::saveProfile(tempPath, d_->profile);
    d_->profileLock = std::make_unique<core::storage::FileLock>(tempPath, 0);
    d_->profileLock->acquire();
    d_->tempProfile = true;
    d_->profilePath = tempPath;
    setStatus(QStringLiteral("Using temporary profile copy"));
    return;
  }
  d_->profileLock = std::move(lock);
  d_->tempProfile = false;
  loadProfileFile(path);
}

void MainWindow::resetProfile() {
  discardTempProfile();
  closeSessionLog();
  d_->profileLock.reset();
  d_->profilePath.clear();
  d_->tempProfile = false;
  core::storage::setCurrentProfilePath(QString());
  d_->profileButton->setText(QStringLiteral("Profile"));
  d_->source->clear();
  d_->destination->clear();
  d_->profile = core::SyncProfile{};
  setStatus(QStringLiteral("Profile reset"));
}

void MainWindow::updateProfileMenu() {
  auto* menu = d_->profileButton->menu();
  menu->clear();
  const QString name = d_->profilePath.isEmpty()
                           ? QStringLiteral("no profile")
                           : QFileInfo(d_->profilePath).fileName();
  auto* heading = menu->addAction(name);
  heading->setEnabled(false);
  connect(menu->addAction(QStringLiteral("Edit...")), &QAction::triggered,
          d_->filtersButton, &QAbstractButton::click);
  menu->addSeparator();
  connect(menu->addAction(QStringLiteral("Save")), &QAction::triggered,
          this, &MainWindow::saveCurrentProfile);
  connect(menu->addAction(QStringLiteral("Save As...")), &QAction::triggered,
          this, &MainWindow::saveProfileAs);
  connect(menu->addAction(QStringLiteral("Load...")), &QAction::triggered,
          this, &MainWindow::loadProfile);
  menu->addSeparator();
  for (const QString& file : core::storage::listProfiles()) {
    const QString path = core::paths::profilesDirectory() + '/' + file;
    connect(menu->addAction(file), &QAction::triggered, this,
            [this, path]() {
              d_->profileLock.reset();
              auto lock = std::make_unique<core::storage::FileLock>(path, 0);
              if (lock->acquire()) {
                d_->profileLock = std::move(lock);
                d_->tempProfile = false;
                loadProfileFile(path);
              }
            });
  }
  menu->addSeparator();
  connect(menu->addAction(QStringLiteral("Reset")), &QAction::triggered,
          this, &MainWindow::resetProfile);
}

}  // namespace qt_rsync::ui
