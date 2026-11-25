#include "ui/MainWindow.h"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

#include <csignal>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QTime>
#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QTimer>
#include <QComboBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/Command.h"
#include "core/Filter.h"
#include "core/Parser.h"
#include "core/Paths.h"
#include "core/Storage.h"
#include "ui/ComparisonView.h"
#include "ui/FilterEditorWidget.h"
#include "ui/OverviewPane.h"
#include "ui/ProfileEditorDialog.h"
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

QFrame* verticalSeparator(QWidget* parent) {
  auto* line = new QFrame(parent);
  line->setFrameShape(QFrame::VLine);
  line->setFrameShadow(QFrame::Plain);
  line->setFixedWidth(1);
  line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  return line;
}

QHash<QString, QPair<qlonglong, QString>> parseListingMetadata(
    const QString& text) {
  QHash<QString, QPair<qlonglong, QString>> metadata;
  static const QRegularExpression line(
      QStringLiteral("^\\S+\\s+([\\d,]+)\\s+(\\d{4}/\\d{2}/\\d{2})\\s+"
                     "(\\d{2}:\\d{2}:\\d{2})\\s+(.+)$"));
  for (const QString& entry : text.split('\n')) {
    const auto match = line.match(entry.trimmed());
    if (!match.hasMatch()) continue;
    QString path = match.captured(4).trimmed();
    while (path.endsWith('/') && path.size() > 1) path.chop(1);
    bool ok = false;
    const qlonglong size = match.captured(1).remove(',').toLongLong(&ok);
    metadata.insert(path,
                    {ok ? size : 0,
                     match.captured(2) + QLatin1Char(' ') + match.captured(3)});
  }
  return metadata;
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
  QToolButton* sourceOpen = nullptr;
  QToolButton* destinationOpen = nullptr;
  QToolButton* compare = nullptr;
  QToolButton* verify = nullptr;
  QToolButton* sync = nullptr;
  QToolButton* undo = nullptr;
  QToolButton* cancel = nullptr;
  QToolButton* preserve = nullptr;
  QToolButton* inventoryAfter = nullptr;
  QToolButton* filtersButton = nullptr;
  QToolButton* settingsButton = nullptr;
  QToolButton* pauseButton = nullptr;
  QToolButton* layoutButton = nullptr;
  QSplitter* mainSplitter = nullptr;
  QWidget* logPanel = nullptr;
  QSplitter* logSplitter = nullptr;
  QString layoutMode = QStringLiteral("normal");
  QAction* undoAction = nullptr;
  QAction* redoAction = nullptr;
  QAction* verifyAction = nullptr;
  QComboBox* mode = nullptr;
  QLineEdit* search = nullptr;
  QLabel* searchStatus = nullptr;
  QToolButton* searchPreviousButton = nullptr;
  QToolButton* searchNextButton = nullptr;
  QToolButton* appMenuButton = nullptr;
  QStringList searchMatches;
  QString searchText;
  int searchPosition = -1;
  bool cancelled = false;
  QLineEdit* status = nullptr;
  QLabel* stateLabel = nullptr;
  QLabel* progressLabel = nullptr;
  QPlainTextEdit* commandPreview = nullptr;
  ActionFilterBar* filterBar = nullptr;
  QPlainTextEdit* journal = nullptr;
  QStringList journalLines;
  QProgressBar* progress = nullptr;
  ComparisonView* comparison = nullptr;
  OverviewPane* overview = nullptr;
  QProcess* compareProcess = nullptr;
  QProcess* operationProcess = nullptr;
  QProcess* inventoryProcess = nullptr;
  QProcess* destinationInventoryProcess = nullptr;
  QVector<core::Change> inventoryChanges;
  QHash<QString, QPair<qlonglong, QString>> destinationListingCache;
  QTemporaryFile* filterFile = nullptr;
  core::SyncProfile profile;
  QVector<core::Change> changes;
  core::ExecutionPlan plan;
  bool verified = false;
  bool stale = false;
  bool syncing = false;
  bool verifying = false;
  QString operationOutput;
  QElapsedTimer compareClock;
  QElapsedTimer viewClock;
  double lastCompareSeconds = 0.0;
  int pendingCompareChanges = 0;
  bool comparisonAwaitingMetadata = false;
  bool destinationInventoryForUnchanged = false;
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

  // Row 1: application controls. qpy builds a plain row of 40x40
  // QToolButtons. A QToolBar auto-raises its buttons, and under Breeze that
  // leaves no visible frame, so the toolbar blended into the window.
  auto* toolbarRow = new QWidget(root);
  auto* toolbarLayout = new QHBoxLayout(toolbarRow);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(4);
  rootLayout->addWidget(toolbarRow);

  const auto styleTool = [](QToolButton* button) {
    button->setIconSize(QSize(24, 24));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(40, 40);
  };

  d_->mode = new QComboBox(toolbarRow);
  d_->mode->addItem(QStringLiteral("Update"), static_cast<int>(core::SyncMode::Update));
  d_->mode->addItem(QStringLiteral("Mirror"), static_cast<int>(core::SyncMode::Mirror));
  d_->mode->addItem(QStringLiteral("New Only"), static_cast<int>(core::SyncMode::NewOnly));
  d_->mode->setToolTip(
      QStringLiteral("Mode selection\n"
                     "Update: copy new and changed items; keep destination-only "
                     "items.\n"
                     "Mirror: make Destination match Source, including "
                     "deletions.\n"
                     "New Only: copy missing items; do not update or delete "
                     "existing items."));
  connect(d_->mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            const auto mode =
                static_cast<core::SyncMode>(d_->mode->itemData(index).toInt());
            d_->profile.transfer.mode = mode;
            if (!d_->changes.isEmpty()) {
              d_->stale = true;
              d_->verified = false;
              d_->sync->setEnabled(false);
              d_->verify->setEnabled(false);
              updateStateIcons();
              setStatus(QStringLiteral(
                  "Stale comparison: synchronization mode changed. Compare "
                  "again to refresh results."));
            } else if (mode == core::SyncMode::Mirror) {
              setStatus(QStringLiteral(
                  "Mirror can delete items that exist only in Destination. "
                  "Click Compare and review deletions."));
            }
          });
  toolbarLayout->addWidget(d_->mode);

  auto* swapButton = new QToolButton(toolbarRow);
  swapButton->setIcon(icons::toolbarIcon(QStringLiteral("swap")));
  swapButton->setToolTip(QStringLiteral("Swap Source and Destination"));
  swapButton->setAccessibleName(QStringLiteral("Swap Source and Destination"));
  styleTool(swapButton);
  connect(swapButton, &QToolButton::clicked, this, [this]() {
    const QString source = d_->source->text();
    d_->source->setText(d_->destination->text());
    d_->destination->setText(source);
  });
  // qpy adds the swap button to this row first and then reparents it into
  // the Source panel below; doing the same keeps the order identical.
  toolbarLayout->addWidget(swapButton);

  const auto addTool = [&](QToolButton*& button, const QString& text,
                           const QString& iconName, const QString& tip) {
    button = new QToolButton(toolbarRow);
    button->setText(text);
    button->setIcon(icons::toolbarIcon(iconName));
    button->setAccessibleName(text);
    styleTool(button);
    button->setToolTip(tip);
    toolbarLayout->addWidget(button);
  };
  addTool(
      d_->compare, QStringLiteral("Compare"), QStringLiteral("compare"),
      QStringLiteral(
          "Compare Source and Destination using an rsync dry run.\n"
          "No files are changed.\n"
          "For SSH endpoints this connects to the remote host, so the remote "
          "host must be reachable and credentials must be available.\n\n"
          "Shortcut: F5"));
  addTool(d_->preserve, QStringLiteral("Preserve"), QStringLiteral("undo"),
          QStringLiteral(
              "Preserve decisions: ON\n"
              "Manual include and exclude decisions survive the next "
              "comparison, even when the reported change of an item changed.\n"
              "Click to turn it off and start fresh; Compare will then ask "
              "for confirmation before discarding the decisions."));
  d_->preserve->setCheckable(true);
  d_->preserve->setChecked(true);
  addTool(
      d_->inventoryAfter, QStringLiteral("Inventory"),
      QStringLiteral("inventory"),
      QStringLiteral(
          "Run an inventory after every comparison.\n"
          "The inventory lists Source and Destination and adds the unchanged "
          "items to the comparison, so they are shown together with the "
          "changed ones. The unchanged button then reuses this result "
          "instead of running the inventory a second time."));
  d_->inventoryAfter->setCheckable(true);
  toolbarLayout->addWidget(verticalSeparator(toolbarRow));
  addTool(
      d_->verify, QStringLiteral("Verify"), QStringLiteral("verify"),
      QStringLiteral(
          "Optional diagnostic: check the selected decisions against the "
          "plan.\n"
          "Synchronization does not depend on this check.\n"
          "When no checkbox decisions were changed after the comparison, the "
          "latest comparison is reused immediately.\n"
          "Otherwise a second rsync dry run checks the selected subset; that "
          "run can be cancelled.\n"
          "For SSH endpoints the dry run connects to the remote host again, "
          "so the remote host must be reachable and credentials must be "
          "available.\n\n"
          "Shortcut: F8"));
  addTool(
      d_->sync, QStringLiteral("Synchronize"), QStringLiteral("sync"),
      QStringLiteral(
          "Open the confirmation dialog and review the exact rsync command, "
          "decision rules and counts before writing. Verification is an "
          "optional diagnostic and is not required to synchronize.\n\n"
          "Shortcut: F9"));
  addTool(d_->cancel, QStringLiteral("Cancel"), QStringLiteral("cancel"),
          QStringLiteral(
              "Cancel the running operation\n\nShortcut: F10"));
  d_->pauseButton = new QToolButton(toolbarRow);
  d_->pauseButton->setText(QStringLiteral("Pause"));
  d_->pauseButton->setAccessibleName(
      QStringLiteral("Pause the running operation"));
  d_->pauseButton->setIcon(icons::toolbarIcon(QStringLiteral("pause")));
  d_->pauseButton->setCheckable(true);
  d_->pauseButton->setToolTip(
      QStringLiteral(
          "Pause the running rsync operation.\n"
          "The transfer is suspended and continues with the same button.\n"
          "For a remote endpoint the connection stays open and the transfer "
          "resumes where it stopped.\n\n"
          "Shortcut: F11"));
  connect(d_->pauseButton, &QToolButton::toggled, this, [this](bool checked) {
    QProcess* active = nullptr;
    if (d_->operationProcess != nullptr &&
        d_->operationProcess->processId() > 0) {
      active = d_->operationProcess;
    } else if (d_->compareProcess != nullptr &&
               d_->compareProcess->processId() > 0) {
      active = d_->compareProcess;
    }
    const qint64 pid = active != nullptr ? active->processId() : 0;
    if (pid > 0) {
      ::kill(static_cast<pid_t>(pid), checked ? SIGSTOP : SIGCONT);
    }
    d_->pauseButton->setIcon(icons::toolbarIcon(
        checked ? QStringLiteral("sync") : QStringLiteral("pause")));
    d_->pauseButton->setToolTip(
        checked
            ? QStringLiteral("Continue the paused rsync operation.\n\n"
                             "Shortcut: F11")
            : QStringLiteral(
                  "Pause the running rsync operation.\n"
                  "The transfer is suspended and continues with the same "
                  "button.\n"
                  "For a remote endpoint the connection stays open and the "
                  "transfer resumes where it stopped.\n\n"
                  "Shortcut: F11"));
    appendJournalLine(checked ? QStringLiteral("Paused")
                              : QStringLiteral("Continuing..."));
  });
  styleTool(d_->pauseButton);
  toolbarLayout->addWidget(d_->pauseButton);

  toolbarLayout->addWidget(verticalSeparator(toolbarRow));
  addTool(d_->filtersButton, QStringLiteral("Filters"), QStringLiteral("filters"),
          QStringLiteral(
              "Edit the persistent filter rules\n\nShortcut: F7"));

  d_->profileButton = new QToolButton(toolbarRow);
  d_->profileButton->setIcon(icons::toolbarIcon(QStringLiteral("profile")));
  d_->profileButton->setAccessibleName(QStringLiteral("Manage Profiles"));
  d_->profileButton->setMenu(new QMenu(d_->profileButton));
  d_->profileButton->setPopupMode(QToolButton::InstantPopup);
  d_->profileButton->setToolTip(
      QStringLiteral("Manage profiles (F2/F3, edit F4)"));
  connect(d_->profileButton->menu(), &QMenu::aboutToShow,
          this, &MainWindow::updateProfileMenu);
  styleTool(d_->profileButton);
  toolbarLayout->addWidget(d_->profileButton);
  auto* backupButton = new QToolButton(toolbarRow);
  backupButton->setIcon(icons::toolbarIcon(QStringLiteral("backup")));
  backupButton->setAccessibleName(QStringLiteral("Open the backup directory"));
  backupButton->setToolTip(QStringLiteral("Open the backup directory"));
  connect(backupButton, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->profile.backup.directory));
  });
  styleTool(backupButton);
  toolbarLayout->addWidget(backupButton);
  const auto configureOpenMenu = [this](QToolButton* button, bool logging) {
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QToolButton::customContextMenuRequested, this,
            [this, button, logging](const QPoint& point) {
              QMenu menu(button);
              QAction* open = menu.addAction(button->toolTip());
              QAction* clear = logging ? menu.addAction(QStringLiteral("Clear Log"))
                                       : nullptr;
              QAction* configure =
                  menu.addAction(QStringLiteral("Configure..."));
              QAction* selected = menu.exec(button->mapToGlobal(point));
              if (selected == open) {
                button->click();
              } else if (selected == clear) {
                if (d_->logFile != nullptr) {
                  d_->logFile->close();
                  d_->logFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
                  d_->logFile->close();
                  d_->logFile->open(QIODevice::WriteOnly | QIODevice::Append);
                }
                setStatus(QStringLiteral("Log files cleared."));
              } else if (selected == configure) {
                if (logging) {
                  SettingsDialog dialog(this);
                  if (dialog.exec() == QDialog::Accepted) {
                    applyActionColours();
                    updateVerificationVisibility();
                  }
                } else {
                  ProfileEditorDialog dialog(
                      d_->profile, this, QStringLiteral("backup"));
                  if (dialog.exec() == QDialog::Accepted) {
                    const core::SyncProfile updated = dialog.profile();
                    d_->profile.backup = updated.backup;
                    d_->profile.transfer = updated.transfer;
                    d_->profile.preservation = updated.preservation;
                    updateStateIcons();
                  }
                }
              }
            });
  };
  configureOpenMenu(backupButton, false);
  auto* logButton = new QToolButton(toolbarRow);
  logButton->setIcon(icons::toolbarIcon(QStringLiteral("log")));
  logButton->setAccessibleName(QStringLiteral("Open the log file"));
  logButton->setToolTip(QStringLiteral("Open the log file"));
  connect(logButton, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->logPath));
  });
  styleTool(logButton);
  toolbarLayout->addWidget(logButton);
  configureOpenMenu(logButton, true);
  addTool(d_->settingsButton, QStringLiteral("Settings"),
          QStringLiteral("settings"), QStringLiteral("Settings"));
  connect(d_->settingsButton, &QAbstractButton::clicked, this, [this]() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
      applyActionColours();
      updateVerificationVisibility();
    }
  });
  connect(d_->filtersButton, &QAbstractButton::clicked, this, [this]() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Filters"));
    dialog.resize(640, 560);
    auto* layout = new QVBoxLayout(&dialog);
    auto* editor = new FilterEditorWidget(&dialog, d_->profile.filterText);
    layout->addWidget(editor, 1);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
      if (editor->firstError().has_value()) {
        editor->revealError();
        QMessageBox::warning(
            &dialog, QStringLiteral("Invalid filters"),
            QStringLiteral("Correct the highlighted filter line first."));
        return;
      }
      dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
      d_->profile.filterText = editor->text();
      d_->profile.filters = editor->rules();
      d_->stale = true;
      updateStateIcons();
      setStatus(QStringLiteral(
          "Stale comparison: filter rules changed. Compare again to refresh "
          "results."));
    }
  });

  toolbarLayout->addWidget(verticalSeparator(toolbarRow));
  auto* findButton = new QToolButton(toolbarRow);
  findButton->setIcon(icons::toolbarIcon(QStringLiteral("search")));
  findButton->setAccessibleName(QStringLiteral("Find next"));
  findButton->setToolTip(QStringLiteral("Find next (Ctrl+F)"));
  styleTool(findButton);
  connect(findButton, &QToolButton::clicked, this, &MainWindow::findNext);
  toolbarLayout->addWidget(findButton);
  d_->searchPreviousButton = new QToolButton(toolbarRow);
  d_->searchPreviousButton->setIcon(
      icons::toolbarIcon(QStringLiteral("search-previous")));
  d_->searchPreviousButton->setAccessibleName(
      QStringLiteral("Previous search match"));
  d_->searchPreviousButton->setToolTip(
      QStringLiteral("Previous match (Ctrl+Shift+G)"));
  styleTool(d_->searchPreviousButton);
  d_->searchPreviousButton->hide();
  connect(d_->searchPreviousButton, &QToolButton::clicked, this,
          &MainWindow::findPrevious);
  toolbarLayout->addWidget(d_->searchPreviousButton);
  d_->searchNextButton = new QToolButton(toolbarRow);
  d_->searchNextButton->setIcon(
      icons::toolbarIcon(QStringLiteral("search-next")));
  d_->searchNextButton->setAccessibleName(
      QStringLiteral("Next search match"));
  d_->searchNextButton->setToolTip(QStringLiteral("Next match (Ctrl+G)"));
  styleTool(d_->searchNextButton);
  d_->searchNextButton->hide();
  connect(d_->searchNextButton, &QToolButton::clicked, this,
          &MainWindow::findNext);
  toolbarLayout->addWidget(d_->searchNextButton);
  d_->search = new QLineEdit(toolbarRow);
  d_->search->setObjectName(QStringLiteral("search-edit"));
  d_->search->setPlaceholderText(QStringLiteral("Search the list..."));
  d_->search->setClearButtonEnabled(true);
  d_->search->setMinimumWidth(120);
  d_->search->setToolTip(
      QStringLiteral(
          "Find items whose name contains the text; use a slash to match "
          "the path, a trailing slash to match a directory name exactly, and "
          "* ? [ ] as globs. So 'dir/' finds only the folder named dir, "
          "while 'dir/a' finds paths ending in dir/a and '*dir/*.txt' finds "
          "matching files. Enter jumps to the next match, Shift+Enter to the "
          "previous one, Ctrl+G repeats it from anywhere. Searching does not "
          "change the list."));
  toolbarLayout->addWidget(d_->search, 1);
  d_->searchStatus = new QLabel(toolbarRow);
  d_->searchStatus->setObjectName(QStringLiteral("search-status"));
  d_->searchStatus->setToolTip(
      QStringLiteral("Search matches in the visible list"));
  d_->searchStatus->setMinimumWidth(52);
  d_->searchStatus->hide();
  toolbarLayout->addWidget(d_->searchStatus);

  auto* layoutButton = new QToolButton(toolbarRow);
  d_->layoutButton = layoutButton;
  layoutButton->setIcon(icons::toolbarIcon(QStringLiteral("layout")));
  layoutButton->setAccessibleName(QStringLiteral("Compare layout"));
  layoutButton->setToolTip(QStringLiteral("Compare layout"));
  auto* layoutMenu = new QMenu(layoutButton);
  const QVector<QPair<QString, QString>> layoutModes{
      {QStringLiteral("Normal"), QStringLiteral("normal")},
      {QStringLiteral("Compare only"), QStringLiteral("focus")},
      {QStringLiteral("Compare with overview"), QStringLiteral("wide")},
  };
  auto* layoutGroup = new QActionGroup(this);
  layoutGroup->setExclusive(true);
  for (const auto& mode : layoutModes) {
    auto* action = layoutMenu->addAction(mode.first);
    action->setCheckable(true);
    action->setChecked(mode.second == QStringLiteral("normal"));
    layoutGroup->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, mode]() { applyLayoutMode(mode.second); });
  }
  layoutButton->setMenu(layoutMenu);
  layoutButton->setPopupMode(QToolButton::InstantPopup);
  styleTool(layoutButton);
  toolbarLayout->addWidget(layoutButton);

  d_->appMenuButton = new QToolButton(toolbarRow);
  d_->appMenuButton->setIcon(icons::toolbarIcon(QStringLiteral("appmenu")));
  d_->appMenuButton->setAccessibleName(QStringLiteral("Application menu"));
  d_->appMenuButton->setToolTip(QStringLiteral("Application menu"));
  d_->appMenuButton->setPopupMode(QToolButton::InstantPopup);
  styleTool(d_->appMenuButton);
  toolbarLayout->addWidget(d_->appMenuButton);

  // Menu bar mirrors the actions available in qpy-rsync.
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("File"));
  auto* loadAction = fileMenu->addAction(QStringLiteral("Load Profile..."));
  loadAction->setShortcut(QKeySequence(Qt::Key_F3));
  connect(loadAction, &QAction::triggered, this, [this]() { loadProfile(); });
  auto* saveAction = fileMenu->addAction(QStringLiteral("Save Profile"));
  saveAction->setShortcut(QKeySequence(Qt::Key_F2));
  connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentProfile);
  auto* saveAsAction = fileMenu->addAction(QStringLiteral("Save Profile As..."));
  connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveProfileAs);
  fileMenu->addSeparator();
  auto* quitAction = fileMenu->addAction(QStringLiteral("Quit"));
  quitAction->setShortcut(QKeySequence::Quit);
  connect(quitAction, &QAction::triggered, this, &QWidget::close);

  auto* editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
  d_->undoAction = editMenu->addAction(QStringLiteral("Undo Decision"));
  d_->undoAction->setShortcut(QKeySequence::Undo);
  d_->undoAction->setEnabled(false);
  connect(d_->undoAction, &QAction::triggered, this,
          [this]() { d_->comparison->undoDecision(); });
  d_->redoAction = editMenu->addAction(QStringLiteral("Redo Decision"));
  d_->redoAction->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
  d_->redoAction->setEnabled(false);
  connect(d_->redoAction, &QAction::triggered, this,
          [this]() { d_->comparison->redoDecision(); });
  auto* resetAction = editMenu->addAction(QStringLiteral("Reset Decisions"));
  connect(resetAction, &QAction::triggered, this,
          [this]() { d_->comparison->resetAllDecisions(); });
  editMenu->addSeparator();
  auto* findAction = editMenu->addAction(QStringLiteral("Find in List"));
  findAction->setShortcut(QKeySequence::Find);
  connect(findAction, &QAction::triggered, this, [this]() {
    d_->search->setFocus(Qt::ShortcutFocusReason);
    d_->search->selectAll();
  });
  auto* findNextAction = editMenu->addAction(QStringLiteral("Find Next"));
  findNextAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
  connect(findNextAction, &QAction::triggered, this, &MainWindow::findNext);
  auto* findPreviousAction = editMenu->addAction(QStringLiteral("Find Previous"));
  findPreviousAction->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+G")));
  connect(findPreviousAction, &QAction::triggered, this,
          &MainWindow::findPrevious);
  editMenu->addSeparator();
  auto* profileAction = editMenu->addAction(QStringLiteral("Profile"));
  profileAction->setShortcut(QKeySequence(Qt::Key_F4));
  connect(profileAction, &QAction::triggered, this, &MainWindow::editProfile);
  auto* settingsAction = editMenu->addAction(QStringLiteral("Settings"));
  settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
  connect(settingsAction, &QAction::triggered, d_->settingsButton,
          &QAbstractButton::click);

  auto* viewMenu = menuBar()->addMenu(QStringLiteral("View"));
  const QVector<QPair<QString, QString>> viewLayoutModes{
      {QStringLiteral("Normal"), QStringLiteral("normal")},
      {QStringLiteral("Compare only"), QStringLiteral("focus")},
      {QStringLiteral("Compare with overview"), QStringLiteral("wide")},
  };
  for (const auto& mode : viewLayoutModes) {
    auto* action = viewMenu->addAction(mode.first);
    action->setCheckable(true);
    action->setChecked(mode.second == QStringLiteral("normal"));
    layoutGroup->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, mode]() { applyLayoutMode(mode.second); });
  }

  auto* actionsMenu = menuBar()->addMenu(QStringLiteral("Decisions"));
  auto* resetAllAction =
      actionsMenu->addAction(QStringLiteral("Reset All Decisions"));
  connect(resetAllAction, &QAction::triggered, this,
          [this]() { d_->comparison->resetAllDecisions(); });
  auto* methodAction = actionsMenu->addAction(QStringLiteral("Method"));
  methodAction->setShortcut(QKeySequence(Qt::Key_F12));
  connect(methodAction, &QAction::triggered, this, [this]() {
    d_->mode->setCurrentIndex((d_->mode->currentIndex() + 1) %
                              d_->mode->count());
  });
  actionsMenu->addSeparator();
  auto* compareAction = actionsMenu->addAction(QStringLiteral("Compare"));
  compareAction->setShortcut(QKeySequence(Qt::Key_F5));
  connect(compareAction, &QAction::triggered, this, &MainWindow::startCompare);
  auto* inventoryAction = actionsMenu->addAction(QStringLiteral("Inventory"));
  inventoryAction->setShortcut(QKeySequence(Qt::Key_F6));
  connect(inventoryAction, &QAction::triggered, this,
          [this]() { d_->filterBar->toggleUnchanged(); });
  auto* filtersAction = actionsMenu->addAction(QStringLiteral("Filters"));
  filtersAction->setShortcut(QKeySequence(Qt::Key_F7));
  connect(filtersAction, &QAction::triggered, d_->filtersButton,
          &QAbstractButton::click);
  d_->verifyAction = actionsMenu->addAction(QStringLiteral("Verify Decisions"));
  d_->verifyAction->setShortcut(QKeySequence(Qt::Key_F8));
  connect(d_->verifyAction, &QAction::triggered, this, &MainWindow::verifyPlan);
  auto* syncAction = actionsMenu->addAction(QStringLiteral("Synchronize"));
  syncAction->setShortcut(QKeySequence(Qt::Key_F9));
  connect(syncAction, &QAction::triggered, this, &MainWindow::synchronize);
  auto* cancelAction = actionsMenu->addAction(QStringLiteral("Cancel"));
  cancelAction->setShortcut(QKeySequence(Qt::Key_F10));
  connect(cancelAction, &QAction::triggered, this, &MainWindow::cancelOperation);
  auto* pauseAction = actionsMenu->addAction(QStringLiteral("Pause"));
  pauseAction->setShortcut(QKeySequence(Qt::Key_F11));
  connect(pauseAction, &QAction::triggered, d_->pauseButton,
          &QAbstractButton::toggle);
  actionsMenu->addSeparator();
  actionsMenu->addAction(QStringLiteral("Collapse All Folders"), this,
                         [this]() { d_->comparison->foldAll(true); });
  actionsMenu->addAction(QStringLiteral("Expand All Folders"), this,
                         [this]() { d_->comparison->foldAll(false); });

  const auto showHowTo = [this]() {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("How to use qt-rsync"));
    box.setTextFormat(Qt::RichText);
    box.setText(
        QStringLiteral(
            "<b>1. Choose Source and Destination.</b><br>"
            "Use local paths or <code>[user@]host:/absolute/path</code>. "
            "At most one endpoint may be remote.<br><br>"
            "<b>2. Compare.</b> Press F5 or the Compare button. The dry run "
            "never changes files.<br><br>"
            "<b>3. Decide.</b> Click the checkbox in the Decisions pane, or "
            "press Space on selected rows. Right-click for all choices. "
            "Excluded items are not synchronized.<br><br>"
            "<b>4. Synchronize.</b> Press F9, review the command and rules, "
            "then confirm. The comparison becomes stale after writing.<br><br>"
            "F7 filters, F8 optional verification, F10 cancel, F11 pause."));
    box.exec();
  };
  auto* helpMenu = menuBar()->addMenu(QStringLiteral("Help"));
  auto* howToAction = helpMenu->addAction(QStringLiteral("How-To"));
  howToAction->setShortcut(QKeySequence(Qt::Key_F1));
  connect(howToAction, &QAction::triggered, this, showHowTo);
  auto* aboutAction = helpMenu->addAction(QStringLiteral("About qt-rsync"));
  connect(aboutAction, &QAction::triggered, this, [this]() {
    QMessageBox::about(
        this, QStringLiteral("About qt-rsync"),
        QStringLiteral(
            "<b>qt-rsync</b> %1<br><br>"
            "A pure Qt/C++ frontend for rsync, ported from qpy-rsync.<br>"
            "Decisions, execution plans and the data store live in C++; "
            "rsync performs the comparison and transfer.")
            .arg(QCoreApplication::applicationVersion()));
  });

  // Mirror the menu bar in the toolbar button, as qpy does for desktops
  // without a global menu.
  auto* appMenu = new QMenu(d_->appMenuButton);
  for (QAction* top : menuBar()->actions()) {
    QMenu* submenu = top->menu();
    if (!submenu) continue;
    QMenu* mirror = appMenu->addMenu(
        QString(submenu->title()).remove(QLatin1Char('&')));
    for (QAction* item : submenu->actions()) {
      if (item->isSeparator()) {
        mirror->addSeparator();
      } else {
        mirror->addAction(item);
      }
    }
  }
  d_->appMenuButton->setMenu(appMenu);
  connect(d_->search, &QLineEdit::returnPressed, this,
          &MainWindow::findNext);
  connect(d_->search, &QLineEdit::textChanged, this, [this](const QString&) {
    refreshSearchMatches();
  });
  // Row 2: source + action filter bar + destination.
  auto* locations = new QHBoxLayout();
  auto* sourcePanel = new QWidget(root);
  auto* sourceLayout = new QHBoxLayout(sourcePanel);
  sourceLayout->setContentsMargins(0, 0, 0, 0);
  sourceLayout->setSpacing(4);
  sourceLayout->addWidget(swapButton);
  auto* sourceOpen = new QToolButton(sourcePanel);
  d_->sourceOpen = sourceOpen;
  sourceOpen->setIcon(icons::toolbarIcon(QStringLiteral("cloud-upload")));
  sourceOpen->setAccessibleName(QStringLiteral("Open the Source directory"));
  sourceOpen->setToolTip(QStringLiteral("Open the Source directory"));
  styleTool(sourceOpen);
  connect(sourceOpen, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->source->text()));
  });
  sourceLayout->addWidget(sourceOpen);
  d_->source = new QLineEdit(sourcePanel);
  d_->source->setObjectName(QStringLiteral("source-edit"));
  const QString endpointPlaceholder =
      QStringLiteral(
          "Local: /path or ~/path    SSH: [user@]host:/absolute/path");
  d_->source->setPlaceholderText(endpointPlaceholder);
  const QString endpointHelp =
      QStringLiteral(
          "Local examples: /home/alice/data or ~/data\n"
          "SSH examples: server:/srv/data or alice@server:/srv/data\n"
          "SSH host aliases from ~/.ssh/config are supported. At most one "
          "endpoint may be remote.");
  d_->source->setToolTip(endpointHelp);
  sourceLayout->addWidget(d_->source, 1);
  auto* sourceBrowse = new QToolButton(sourcePanel);
  sourceBrowse->setIcon(icons::toolbarIcon(QStringLiteral("folder-add")));
  sourceBrowse->setAccessibleName(QStringLiteral("Browse to Source location"));
  sourceBrowse->setToolTip(QStringLiteral("Browse to Source location"));
  styleTool(sourceBrowse);
  connect(sourceBrowse, &QToolButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Browse to Source location"));
    if (!dir.isEmpty()) d_->source->setText(dir);
  });
  sourceLayout->addWidget(sourceBrowse);
  sourceLayout->addWidget(verticalSeparator(sourcePanel));

  auto* filterColumn = new QWidget(root);
  auto* filterLayout = new QVBoxLayout(filterColumn);
  filterLayout->setContentsMargins(0, 0, 0, 0);
  filterLayout->setSpacing(0);
  filterLayout->setAlignment(Qt::AlignBottom);
  d_->filterBar = new ActionFilterBar(filterColumn);
  filterLayout->addWidget(d_->filterBar);

  auto* destinationPanel = new QWidget(root);
  auto* destinationLayout = new QHBoxLayout(destinationPanel);
  destinationLayout->setContentsMargins(0, 0, 0, 0);
  destinationLayout->setSpacing(4);
  auto* destinationOpen = new QToolButton(destinationPanel);
  d_->destinationOpen = destinationOpen;
  destinationOpen->setIcon(icons::toolbarIcon(QStringLiteral("cloud-download")));
  destinationOpen->setAccessibleName(
      QStringLiteral("Open the Destination directory"));
  destinationOpen->setToolTip(QStringLiteral("Open the Destination directory"));
  styleTool(destinationOpen);
  connect(destinationOpen, &QToolButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(d_->destination->text()));
  });
  destinationLayout->addWidget(destinationOpen);
  d_->destination = new QLineEdit(destinationPanel);
  d_->destination->setObjectName(QStringLiteral("destination-edit"));
  d_->destination->setPlaceholderText(endpointPlaceholder);
  d_->destination->setToolTip(endpointHelp);
  destinationLayout->addWidget(d_->destination, 1);
  auto* destinationBrowse = new QToolButton(destinationPanel);
  destinationBrowse->setIcon(icons::toolbarIcon(QStringLiteral("folder-add")));
  destinationBrowse->setAccessibleName(
      QStringLiteral("Browse to Destination location"));
  destinationBrowse->setToolTip(
      QStringLiteral("Browse to Destination location"));
  styleTool(destinationBrowse);
  connect(destinationBrowse, &QToolButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Browse to Destination location"));
    if (!dir.isEmpty()) d_->destination->setText(dir);
  });
  destinationLayout->addWidget(destinationBrowse);

  locations->addWidget(sourcePanel, 40);
  locations->addWidget(filterColumn, 0, Qt::AlignBottom);
  locations->addWidget(destinationPanel, 40);
  rootLayout->addLayout(locations);
  // External open buttons only work for local endpoints; qpy disables them
  // for SSH operands instead of opening a bogus local path.
  const auto updateOpenStates = [this]() {
    const auto local = [](const QString& text) {
      if (text.trimmed().isEmpty()) return false;
      return core::parseEndpoint(text).kind == core::EndpointKind::Local;
    };
    d_->sourceOpen->setEnabled(local(d_->source->text()));
    d_->destinationOpen->setEnabled(local(d_->destination->text()));
  };
  connect(d_->source, &QLineEdit::textChanged, this,
          [updateOpenStates](const QString&) { updateOpenStates(); });
  connect(d_->destination, &QLineEdit::textChanged, this,
          [updateOpenStates](const QString&) { updateOpenStates(); });
  connect(d_->destination, &QLineEdit::returnPressed, this,
          [this]() { startCompare(); });
  updateOpenStates();

  // Row 3: comparison above, journal + overview below.
  auto* splitter = new QSplitter(Qt::Vertical, root);
  d_->mainSplitter = splitter;
  d_->comparison = new ComparisonView(splitter);
  d_->comparison->setFilterBar(d_->filterBar);
  splitter->addWidget(d_->comparison);

  auto* logPanel = new QWidget(splitter);
  d_->logPanel = logPanel;
  auto* logPanelLayout = new QHBoxLayout(logPanel);
  logPanelLayout->setContentsMargins(0, 0, 0, 0);
  logPanelLayout->setSpacing(4);
  auto* logSplitter = new QSplitter(Qt::Horizontal, logPanel);
  d_->logSplitter = logSplitter;
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
  d_->commandPreview->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  d_->commandPreview->setWordWrapMode(
      QTextOption::WrapAtWordBoundaryOrAnywhere);
  d_->commandPreview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d_->commandPreview->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  d_->commandPreview->setTabChangesFocus(true);
  d_->commandPreview->setMinimumWidth(0);
  d_->commandPreview->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Preferred);
  // Grow with the command, but cap at three lines like qpy; longer commands
  // scroll vertically instead of pushing the journal out of the window.
  d_->commandPreview->setMinimumHeight(
      d_->commandPreview->fontMetrics().lineSpacing() + 12);
  d_->commandPreview->setMaximumHeight(
      d_->commandPreview->fontMetrics().lineSpacing() * 3 + 12);
  d_->commandPreview->setPlaceholderText(QStringLiteral("Command preview appears here."));
  logColumnLayout->addWidget(d_->commandPreview);

  d_->journal = new QPlainTextEdit(logColumn);
  d_->journal->setReadOnly(true);
  d_->journal->setMaximumBlockCount(5000);
  d_->journal->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  logColumnLayout->addWidget(d_->journal, 1);
  logSplitter->addWidget(logColumn);
  d_->overview = new OverviewPane(logSplitter);
  connect(d_->overview, &OverviewPane::folderActivated, this,
          [this](const QString& path) {
            d_->comparison->activatePath(path);
          });
  connect(d_->overview, &OverviewPane::excludeRequested, this,
          [this](const QString& path) {
            d_->comparison->applyDecision(
                path, static_cast<int>(core::Decision::Exclude));
          });
  connect(d_->overview, &OverviewPane::restoreRequested, this,
          [this](const QString& path) {
            d_->comparison->applyDecision(
                path, static_cast<int>(core::Decision::Inherit));
          });
  connect(d_->overview, &OverviewPane::filterRequested, this,
          [this](const QString& pattern, const QString& path) {
            addFilterPattern(pattern, path);
          });
  logSplitter->setStretchFactor(0, 1);
  logSplitter->setStretchFactor(1, 0);
  logSplitter->setSizes({680, 620});
  logPanelLayout->addWidget(logSplitter);
  splitter->addWidget(logPanel);
  splitter->setSizes({500, 280});
  rootLayout->addWidget(splitter, 1);

  // Status bar, mirroring qpy: a "State:" label and a frameless read-only
  // field whose colour follows the severity of the last state.
  d_->stateLabel = new QLabel(QStringLiteral("State:"), this);
  d_->status = new QLineEdit(this);
  d_->status->setObjectName(QStringLiteral("status-label"));
  d_->status->setReadOnly(true);
  d_->status->setFrame(false);
  d_->status->setCursorPosition(0);
  d_->status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  statusBar()->setSizeGripEnabled(false);
  statusBar()->addWidget(d_->stateLabel);
  statusBar()->addWidget(d_->status, 1);

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
    updateStateIcons();
  });
  connect(d_->filterBar, &ActionFilterBar::unchangedToggled, this,
          [this](bool enabled) {
            if (enabled) startInventory();
          });
  connect(d_->filterBar, &ActionFilterBar::undoRequested, this,
          [this]() { d_->comparison->undoDecision(); });
  connect(d_->comparison, &ComparisonView::undoAvailable, d_->filterBar,
          &ActionFilterBar::setUndoEnabled);
  connect(d_->comparison, &ComparisonView::undoAvailable, d_->undoAction,
          &QAction::setEnabled);
  connect(d_->comparison, &ComparisonView::redoAvailable, d_->redoAction,
          &QAction::setEnabled);
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
  // QAction shortcuts in the menu bar are not delivered on this KDE setup
  // (the menu is exported to the global menu). Use window-level QShortcuts
  // for the same keys so the keyboard works on every desktop.
  for (QAction* top : menuBar()->actions()) {
    QMenu* submenu = top->menu();
    if (!submenu) continue;
    for (QAction* action : submenu->actions()) {
      action->setShortcut(QKeySequence());
    }
  }
  const auto addKey = [this](const QKeySequence& sequence, auto slot) {
    auto* shortcut = new QShortcut(sequence, this);
    connect(shortcut, &QShortcut::activated, this, slot);
  };
  addKey(QKeySequence(Qt::Key_F1), showHowTo);
  addKey(QKeySequence(Qt::Key_F2), &MainWindow::saveCurrentProfile);
  addKey(QKeySequence(Qt::Key_F3), &MainWindow::loadProfile);
  addKey(QKeySequence(Qt::Key_F4), &MainWindow::editProfile);
  addKey(QKeySequence(Qt::Key_F5), &MainWindow::startCompare);
  addKey(QKeySequence(Qt::Key_F6), [this]() {
    d_->filterBar->toggleUnchanged();
  });
  addKey(QKeySequence(Qt::Key_F7), [this]() { d_->filtersButton->click(); });
  addKey(QKeySequence(Qt::Key_F8), &MainWindow::verifyPlan);
  addKey(QKeySequence(Qt::Key_F9), &MainWindow::synchronize);
  addKey(QKeySequence(Qt::Key_F10), &MainWindow::cancelOperation);
  addKey(QKeySequence(Qt::Key_F11), [this]() {
    d_->pauseButton->toggle();
  });
  addKey(QKeySequence(Qt::Key_F12), [this]() {
    d_->mode->setCurrentIndex((d_->mode->currentIndex() + 1) %
                              d_->mode->count());
  });
  addKey(QKeySequence::Undo, [this]() {
    d_->comparison->undoDecision();
  });
  addKey(QKeySequence(QStringLiteral("Ctrl+Shift+Z")), [this]() {
    d_->comparison->redoDecision();
  });
  addKey(QKeySequence::Find, [this]() {
    d_->search->setFocus(Qt::ShortcutFocusReason);
    d_->search->selectAll();
  });
  addKey(QKeySequence(QStringLiteral("Ctrl+G")), &MainWindow::findNext);
  addKey(QKeySequence(QStringLiteral("Ctrl+Shift+G")),
         &MainWindow::findPrevious);
  addKey(QKeySequence(QStringLiteral("Ctrl+,")), [this]() {
    d_->settingsButton->click();
  });
  {
    core::storage::Settings settings;
    d_->filterBar->setFilterDirectories(
        settings.value(QStringLiteral("comparison/filter-directories"), false)
            .toBool());
  }
  connect(d_->filterBar, &ActionFilterBar::filterChanged, this, [this]() {
    core::storage::Settings settings;
    settings.setValue(QStringLiteral("comparison/filter-directories"),
                      d_->filterBar->filterDirectories());
    settings.sync();
  });
  applyActionColours();
  updateVerificationVisibility();
  updateStateIcons();
  restoreProfile();
  restoreWindowState();

  connect(d_->comparison, &ComparisonView::viewChanged, this, [this]() {
    d_->overview->rebuild(d_->comparison->store());
  });
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
  connect(d_->comparison, &ComparisonView::diffRequested, this,
          [this](const QString& path) {
            if (d_->profile.source.kind != core::EndpointKind::Local ||
                d_->profile.destination.kind != core::EndpointKind::Local) {
              QMessageBox::information(
                  this, QStringLiteral("Diff tool unavailable"),
                  QStringLiteral(
                      "Comparing the contents of remote endpoints requires "
                      "staging the files locally, which this build does not "
                      "do yet."));
              return;
            }
            const QString source =
                QDir(d_->profile.source.path).filePath(path);
            const QString destination =
                QDir(d_->profile.destination.path).filePath(path);
            if (!QFileInfo::exists(source) || !QFileInfo::exists(destination)) {
              QMessageBox::information(
                  this, QStringLiteral("Diff tool unavailable"),
                  QStringLiteral(
                      "Both sides must have the file. Choose a changed item "
                      "that exists on Source and Destination."));
              return;
            }
            core::storage::Settings settings;
            QString tool = QStringLiteral("meld");
            const QString configuredCommand =
                settings.value(QStringLiteral("tools/diff-command"))
                    .toString()
                    .trimmed();
            const QStringList commands =
                settings.value(QStringLiteral("tools/commands")).toStringList();
            const QString command = configuredCommand.isEmpty()
                                        ? (commands.isEmpty() ? QString()
                                                              : commands.first())
                                        : configuredCommand;
            if (!command.isEmpty()) tool = command.section(' ', 0, 0);
            if (!QProcess::startDetached(tool, {source, destination})) {
              QMessageBox::information(
                  this, QStringLiteral("Diff tool unavailable"),
                  QStringLiteral("Could not start %1.").arg(tool));
              return;
            }
            appendJournalLine(QStringLiteral("Opened diff for ") + path);
            setStatus(QStringLiteral("Opened diff for %1").arg(path));
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
  QTextCursor cursor(d_->journal->document());
  cursor.movePosition(QTextCursor::End);
  if (!d_->journal->document()->isEmpty()) cursor.insertBlock();
  QTextCharFormat format;
  format.setForeground(QColor(colour));
  cursor.insertText(line, format);
  d_->journal->setTextCursor(cursor);
  d_->journal->ensureCursorVisible();
  QTimer::singleShot(0, d_->journal, [journal = d_->journal]() {
    auto* bar = journal->verticalScrollBar();
    if (bar != nullptr) bar->setValue(bar->maximum());
  });
}

void MainWindow::setCommandPreview(const core::ProcessSpec& spec) {
  d_->commandPreview->setPlainText(QStringLiteral("$ ") + spec.argv().join(' '));
  d_->commandPreview->setToolTip(d_->commandPreview->toPlainText());
}

void MainWindow::setStatus(const QString& text) {
  const QString lower = text.toLower();
  QString colour;
  if (lower.contains(QStringLiteral("failed")) ||
      lower.contains(QStringLiteral("error"))) {
    colour = QStringLiteral("#B91C1C");
  } else if (lower.contains(QStringLiteral("complete")) ||
             lower.contains(QStringLiteral("synchronized")) ||
             lower.contains(QStringLiteral("verified"))) {
    colour = QStringLiteral("#15803D");
  } else if (lower.contains(QStringLiteral("stale")) ||
             lower.contains(QStringLiteral("cancel"))) {
    colour = QStringLiteral("#EA580C");
  }
  d_->status->setStyleSheet(
      QStringLiteral(
          "background-color: palette(window); border: none; padding: 0px 2px;") +
      (colour.isEmpty() ? QString() : QStringLiteral(" color: %1;").arg(colour)));
  d_->status->setToolTip(text);
  d_->status->setText(text);
  d_->status->setCursorPosition(0);
  const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zz"));
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
  d_->destinationListingCache.clear();
  const core::ProcessSpec spec = core::buildCompareSpec(d_->profile);
  setProcessEnvironment(spec, d_->compareProcess);
  setCommandPreview(spec);
  appendJournalLine(QStringLiteral("$ ") + spec.argv().join(' '));
  appendJournalLine(QStringLiteral("rsync dry run start clock set."));
  appendJournalLine(QStringLiteral("STATE comparing"));
  d_->compareClock.start();
  d_->compare->setEnabled(false);
  d_->cancel->setEnabled(true);
  setStatus(QStringLiteral("Comparing..."));
  d_->pauseButton->setChecked(false);
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
  const double compareSeconds = d_->compareClock.isValid()
                                    ? d_->compareClock.elapsed() / 1000.0 : 0.0;
  appendJournalLine(
      QStringLiteral("Compare parse: %1 line(s), parsing 0.00 s, journal "
                     "writes 0.00 s.")
          .arg(output.count('\n')));
  appendJournalLine(
      QStringLiteral("rsync dry run finished in %1 s (wall %1 s).")
          .arg(compareSeconds, 0, 'f', 2));
  appendJournalLine(d_->preserve->isChecked()
                        ? QStringLiteral("Decisions preserved for this comparison.")
                        : QStringLiteral("Decisions reset for this comparison."));
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
  d_->viewClock.start();
  d_->comparison->setChanges(parsed.changes);
  d_->filterBar->setExcludedCount(0);
  d_->overview->rebuild(d_->comparison->store());
  appendJournalLine(
      QStringLiteral("Comparison view built in %1 s.")
          .arg(d_->viewClock.elapsed() / 1000.0, 0, 'f', 2));
  refreshPlan();
  // The comparison is not complete until the destination metadata listing
  // finishes. Keep Compare disabled and Sync/Verify unavailable meanwhile.
  d_->sync->setEnabled(false);
  d_->verify->setEnabled(false);
  d_->lastCompareSeconds = compareSeconds;
  d_->pendingCompareChanges = parsed.changes.size();
  d_->comparisonAwaitingMetadata = true;
  const auto& summary = d_->plan.summary;
  appendJournalLine(
      QStringLiteral(
          "Selected: %1 items, %2; new %3, updated %4, metadata %5, deleted "
          "%6; kept %7, unchecked %8, modified %9, destructive %10; backup %11")
          .arg(summary.included)
          .arg(model::qpy_format_size(summary.includedSize))
          .arg(summary.creates)
          .arg(summary.updates)
          .arg(summary.metadata)
          .arg(summary.deletions)
          .arg(summary.retainedDestinationOnly)
          .arg(summary.excluded)
          .arg(summary.modified)
          .arg(summary.destructive)
          .arg(summary.backupEnabled ? QStringLiteral("on")
                                     : QStringLiteral("off")));
  // qpy keeps the progress bar hidden for a comparison; it is only shown
  // while verifying or synchronizing. Leaving a bright 100% bar on screen
  // after Compare was a visible mismatch.
  d_->progress->hide();
  d_->progressLabel->setText(QStringLiteral("Gathering destination metadata..."));
  startDestinationInventory();
  setStatus(QStringLiteral("Gathering destination metadata..."));
}

void MainWindow::onCompareError(QProcess::ProcessError error) {
  Q_UNUSED(error)
  d_->compare->setEnabled(true);
  d_->cancel->setEnabled(false);
  setStatus(QStringLiteral("Could not start rsync: %1")
                          .arg(d_->compareProcess->errorString()));
}

void MainWindow::startDestinationInventory() {
  // The post-compare listing fills destination sizes and dates only. It must
  // not be mistaken for the source inventory, and it must not add unchanged
  // rows until the user asks for them.
  d_->destinationInventoryForUnchanged = false;
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.destination);
  appendJournalLine(QStringLiteral("$ ") + spec.argv().join(' '));
  setProcessEnvironment(spec, d_->destinationInventoryProcess);
  d_->destinationInventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::startInventory() {
  d_->inventoryChanges.clear();
  d_->destinationInventoryForUnchanged = false;
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.source);
  appendJournalLine(QStringLiteral("$ ") + spec.argv().join(' '));
  setProcessEnvironment(spec, d_->inventoryProcess);
  d_->inventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::filterUnchangedAgainstDestination() {
  // Unchanged objects are the intersection of the source and destination
  // listings, excluding paths that already changed and their ancestors. The
  // destination listing was cached during Compare, so only Source is listed
  // again when the user asks for unchanged items.
  QSet<QString> changedPaths;
  for (const core::Change& change : d_->changes) {
    changedPaths.insert(change.path);
  }
  QSet<QString> ancestorPaths;
  for (const QString& path : changedPaths) {
    QString parent = path;
    int slash = parent.lastIndexOf('/');
    while (slash > 0) {
      parent = parent.left(slash);
      ancestorPaths.insert(parent);
      slash = parent.lastIndexOf('/');
    }
  }
  QVector<core::Change> filtered;
  for (core::Change change : d_->inventoryChanges) {
    if (changedPaths.contains(change.path) ||
        ancestorPaths.contains(change.path)) {
      continue;
    }
    const auto found = d_->destinationListingCache.constFind(change.path);
    if (found == d_->destinationListingCache.constEnd()) continue;
    change.action = core::Action::NoChange;
    change.destinationSize = found->first;
    change.destinationTime = found->second;
    filtered.append(change);
  }
  d_->inventoryChanges = filtered;
}

void MainWindow::mergeInventoryChanges() {
  if (d_->inventoryChanges.isEmpty()) return;
  QVector<core::Change> merged = d_->changes;
  QSet<QString> present;
  for (const core::Change& existing : merged) present.insert(existing.path);
  for (const core::Change& change : d_->inventoryChanges) {
    if (present.contains(change.path)) continue;
    present.insert(change.path);
    merged.append(change);
  }
  d_->changes = merged;
  d_->comparison->setChanges(merged);
}

void MainWindow::onInventoryFinished(int, QProcess::ExitStatus) {
  if (d_->cancelled) return;
  const QString sourceText = core::cleanQtNoise(
      QString::fromLocal8Bit(d_->inventoryProcess->readAllStandardOutput()));
  d_->inventoryChanges = core::parseInventory(sourceText);
  if (!d_->destinationListingCache.isEmpty()) {
    filterUnchangedAgainstDestination();
    mergeInventoryChanges();
    d_->comparison->applyDestinationMetadata(d_->destinationListingCache);
    d_->overview->rebuild(d_->comparison->store());
    d_->progress->hide();
    return;
  }
  // No cached destination listing is available (for example the metadata
  // listing failed); complete the inventory with a destination listing.
  d_->destinationInventoryForUnchanged = true;
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.destination);
  appendJournalLine(QStringLiteral("$ ") + spec.argv().join(' '));
  setProcessEnvironment(spec, d_->destinationInventoryProcess);
  d_->destinationInventoryProcess->start(spec.program, spec.arguments);
  d_->progress->hide();
}

void MainWindow::onDestinationInventoryFinished(int, QProcess::ExitStatus) {
  if (d_->cancelled) return;
  const QString text = core::cleanQtNoise(
      QString::fromLocal8Bit(d_->destinationInventoryProcess->readAllStandardOutput()));
  const auto metadata = parseListingMetadata(text);
  if (!metadata.isEmpty()) {
    d_->destinationListingCache = metadata;
    appendJournalLine(QStringLiteral("Destination metadata: %1 object(s).")
                          .arg(metadata.size()));
  }
  if (d_->destinationInventoryForUnchanged) {
    d_->destinationInventoryForUnchanged = false;
    filterUnchangedAgainstDestination();
    mergeInventoryChanges();
  }
  if (!d_->destinationListingCache.isEmpty()) {
    d_->comparison->applyDestinationMetadata(d_->destinationListingCache);
    d_->overview->rebuild(d_->comparison->store());
  }
  if (d_->comparisonAwaitingMetadata) {
    d_->comparisonAwaitingMetadata = false;
    d_->stale = false;
    updateStateIcons();
    refreshPlan();
    d_->compare->setEnabled(true);
    d_->cancel->setEnabled(false);
    d_->progress->hide();
    d_->progressLabel->setText(QStringLiteral("Comparison completed"));
    appendJournalLine(QStringLiteral("STATE comparison-complete changes=%1")
                          .arg(d_->pendingCompareChanges));
    setStatus(QStringLiteral("Comparison complete: %1 changed item(s)")
                            .arg(d_->pendingCompareChanges));
    appendJournalLine(
        QStringLiteral("Comparison ready in %1 s.")
            .arg(d_->lastCompareSeconds, 0, 'f', 2));
  }
  d_->progress->hide();
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
  d_->filterFile = new QTemporaryFile(
      QDir::tempPath() + QStringLiteral("/qt-rsync-plan-XXXXXX.rules"), this);
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
  d_->filterFile = new QTemporaryFile(
      QDir::tempPath() + QStringLiteral("/qt-rsync-plan-XXXXXX.rules"), this);
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
  d_->pauseButton->setChecked(false);
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
    updateStateIcons();
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
  updateStateIcons();
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
  if (!d_->changes.isEmpty()) {
    // A new filter changes the comparison's result set. Do not let an old
    // plan be synchronized with a filter the user has not compared yet.
    d_->stale = true;
    d_->verified = false;
    d_->sync->setEnabled(false);
    d_->verify->setEnabled(false);
    updateStateIcons();
    setStatus(QStringLiteral(
        "Stale comparison: added the filter %1. Compare again to refresh "
        "results.").arg(pattern));
  } else {
    setStatus(QStringLiteral("Filter added: %1").arg(pattern));
  }
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
  core::storage::Settings settings;
  const QString configured =
      settings.value(QStringLiteral("logging/file")).toString().trimmed();
  d_->logPath =
      configured.isEmpty() ? core::paths::appLogPath(name) : configured;
  if (!settings.value(QStringLiteral("logging/enabled"), true).toBool()) {
    return;
  }
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
  updateStateIcons();
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
    appendJournalLine(
        QStringLiteral("Profile loaded automatically: %1")
            .arg(QFileInfo(current).completeBaseName()));
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

void MainWindow::applyActionColours() {
  static const QStringList keys{
      QStringLiteral("Create"), QStringLiteral("Update"),
      QStringLiteral("Metadata only"), QStringLiteral("Delete"),
      QStringLiteral("Keep in Destination")};
  static const QStringList defaults{
      QStringLiteral("#208020"), QStringLiteral("#eab308"),
      QStringLiteral("#7c3aed"), QStringLiteral("#ef4444"),
      QStringLiteral("#a05020")};
  core::storage::Settings settings;
  const QJsonObject stored = QJsonDocument::fromJson(
      settings.value(QStringLiteral("colours/actions")).toByteArray()).object();
  QVector<QString> colours;
  for (int index = 0; index < keys.size(); ++index) {
    if (!stored.contains(keys.at(index))) {
      colours.append(defaults.at(index));
      continue;
    }
    const QJsonValue value = stored.value(keys.at(index));
    // A JSON null is the user's "do not mark this action" choice.
    colours.append(value.isNull() ? QString() : value.toString());
  }
  if (d_->comparison != nullptr) d_->comparison->setActionColours(colours);
  if (d_->filterBar != nullptr) d_->filterBar->setActionColours(colours);
  if (d_->overview != nullptr) d_->overview->setActionColours(colours);
}

void MainWindow::applyLayoutMode(const QString& mode) {
  if (d_->mainSplitter == nullptr || d_->logPanel == nullptr ||
      d_->logSplitter == nullptr || d_->overview == nullptr) {
    return;
  }
  d_->layoutMode = mode;
  if (mode == QLatin1String("focus")) {
    d_->mainSplitter->setOrientation(Qt::Vertical);
    d_->logPanel->hide();
    d_->overview->hide();
    d_->mainSplitter->setSizes({1, 0});
    return;
  }
  if (mode == QLatin1String("wide")) {
    d_->mainSplitter->setOrientation(Qt::Horizontal);
    d_->logPanel->hide();
    d_->mainSplitter->addWidget(d_->overview);
    d_->overview->setMinimumWidth(180);
    d_->overview->show();
    // The hidden log panel still occupies a splitter index, so give it zero.
    d_->mainSplitter->setSizes({600, 0, 400});
    return;
  }
  d_->mainSplitter->setOrientation(Qt::Vertical);
  if (d_->overview->parentWidget() != d_->logSplitter) {
    d_->logSplitter->addWidget(d_->overview);
  }
  d_->overview->setMinimumWidth(0);
  d_->logPanel->show();
  d_->overview->show();
  d_->logSplitter->setStretchFactor(0, 1);
  d_->logSplitter->setStretchFactor(1, 0);
  d_->logSplitter->setSizes({680, 620});
  d_->mainSplitter->setSizes({500, 280});
}

void MainWindow::restoreWindowState() {
  core::storage::Settings settings;
  const auto decode = [&settings](const QString& key) {
    const QString encoded = settings.value(key).toString();
    if (encoded.isEmpty()) return QByteArray();
    return QByteArray::fromBase64(encoded.toLatin1());
  };
  const QByteArray geometry = decode(QStringLiteral("main-window/geometry"));
  if (!geometry.isEmpty()) restoreGeometry(geometry);
  const QByteArray state = decode(QStringLiteral("main-window/state"));
  if (!state.isEmpty()) restoreState(state);
  const QString mode =
      settings.value(QStringLiteral("main-window/layout-mode"),
                     QStringLiteral("normal"))
          .toString();
  applyLayoutMode(mode);
  const QByteArray splitter = decode(
      QStringLiteral("main-window/layout-state-") +
      (d_->layoutMode.isEmpty() ? QStringLiteral("normal") : d_->layoutMode));
  if (!splitter.isEmpty()) d_->mainSplitter->restoreState(splitter);
  const QByteArray logSplitter = decode(QStringLiteral("main-window/log-splitter"));
  if (!logSplitter.isEmpty()) d_->logSplitter->restoreState(logSplitter);
}

void MainWindow::saveWindowState() {
  core::storage::Settings settings;
  settings.setValue(QStringLiteral("main-window/geometry"),
                    QString::fromLatin1(saveGeometry().toBase64()));
  settings.setValue(QStringLiteral("main-window/state"),
                    QString::fromLatin1(saveState().toBase64()));
  settings.setValue(QStringLiteral("main-window/layout-mode"), d_->layoutMode);
  settings.setValue(
      QStringLiteral("main-window/layout-state-") + d_->layoutMode,
      QString::fromLatin1(d_->mainSplitter->saveState().toBase64()));
  settings.setValue(QStringLiteral("main-window/log-splitter"),
                    QString::fromLatin1(d_->logSplitter->saveState().toBase64()));
  settings.sync();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  saveWindowState();
  QMainWindow::closeEvent(event);
}

void MainWindow::updateStateIcons() {
  const QColor info(QStringLiteral("#0891B2"));
  const QColor warning(QStringLiteral("#f59e0b"));
  QIcon compare = icons::toolbarIcon(QStringLiteral("compare"));
  if (d_->stale) {
    compare = icons::badgedIcon(compare, warning);
  } else if (!d_->changes.isEmpty()) {
    compare = icons::badgedIcon(compare, info);
  }
  d_->compare->setIcon(compare);

  QIcon preserve = icons::toolbarIcon(QStringLiteral("undo"));
  d_->preserve->setIcon(d_->preserve->isChecked()
                            ? icons::badgedIcon(preserve, info)
                            : preserve);

  QIcon verify = icons::toolbarIcon(QStringLiteral("verify"));
  if (d_->verified) {
    verify = icons::badgedIcon(verify, info);
  } else if (d_->stale) {
    verify = icons::badgedIcon(verify, warning);
  }
  d_->verify->setIcon(verify);

  QIcon profile = icons::toolbarIcon(QStringLiteral("profile"));
  if (!d_->profilePath.isEmpty()) {
    profile = icons::badgedIcon(profile, info);
  }
  d_->profileButton->setIcon(profile);

  QIcon filters = icons::toolbarIcon(QStringLiteral("filters"));
  if (!d_->profile.filterText.trimmed().isEmpty()) {
    filters = icons::badgedIcon(filters, info);
  }
  d_->filtersButton->setIcon(filters);
}

void MainWindow::updateVerificationVisibility() {
  core::storage::Settings settings;
  const bool enabled =
      settings.value(QStringLiteral("verification/enabled"), false).toBool();
  if (d_->verify != nullptr) d_->verify->setVisible(enabled);
  if (d_->verifyAction != nullptr) {
    d_->verifyAction->setVisible(enabled);
    d_->verifyAction->setEnabled(enabled);
  }
}

void MainWindow::refreshSearchMatches() {
  const QString text = d_->search->text();
  d_->searchMatches = d_->comparison->searchMatches(text);
  d_->searchPosition = -1;
  d_->searchText = text.trimmed().toLower();
  const bool hasText = !d_->searchText.isEmpty();
  d_->searchStatus->setVisible(hasText);
  d_->searchPreviousButton->setVisible(hasText && !d_->searchMatches.isEmpty());
  d_->searchNextButton->setVisible(hasText && !d_->searchMatches.isEmpty());
  if (!hasText) {
    d_->searchStatus->clear();
    return;
  }
  d_->searchStatus->setText(
      d_->searchMatches.isEmpty()
          ? QStringLiteral("no match")
          : QStringLiteral("0/%1").arg(d_->searchMatches.size()));
}

void MainWindow::stepSearch(int direction) {
  if (d_->searchMatches.isEmpty()) {
    refreshSearchMatches();
    if (d_->searchMatches.isEmpty()) return;
  }
  const int count = d_->searchMatches.size();
  d_->searchPosition = (d_->searchPosition + direction + count) % count;
  d_->comparison->selectPath(d_->searchMatches.at(d_->searchPosition));
  d_->searchStatus->setText(
      QStringLiteral("%1/%2").arg(d_->searchPosition + 1).arg(count));
}

void MainWindow::findNext() {
  if (d_->search->text().trimmed().isEmpty()) {
    d_->search->setFocus(Qt::ShortcutFocusReason);
    d_->search->selectAll();
    return;
  }
  if (d_->search->text().trimmed().toLower() != d_->searchText ||
      d_->searchMatches.isEmpty()) {
    refreshSearchMatches();
  }
  stepSearch(1);
}

void MainWindow::findPrevious() {
  if (d_->search->text().trimmed().isEmpty()) {
    d_->search->setFocus(Qt::ShortcutFocusReason);
    d_->search->selectAll();
    return;
  }
  if (d_->search->text().trimmed().toLower() != d_->searchText ||
      d_->searchMatches.isEmpty()) {
    refreshSearchMatches();
  }
  stepSearch(-1);
}

void MainWindow::editProfile() {
  const QString previousFilters = d_->profile.filterText;
  ProfileEditorDialog dialog(d_->profile, this);
  if (dialog.exec() != QDialog::Accepted) return;
  const core::SyncProfile updated = dialog.profile();
  d_->profile.transfer = updated.transfer;
  d_->profile.preservation = updated.preservation;
  d_->profile.backup = updated.backup;
  d_->profile.filterText = updated.filterText;
  d_->profile.filters = updated.filters;
  d_->profile.inventoryAfterCompare = updated.inventoryAfterCompare;
  d_->mode->setCurrentIndex(static_cast<int>(d_->profile.transfer.mode));
  d_->inventoryAfter->setChecked(d_->profile.inventoryAfterCompare);
  if (d_->profile.filterText != previousFilters) {
    d_->stale = true;
    updateStateIcons();
    setStatus(QStringLiteral(
        "Stale comparison: filter rules changed. Compare again to refresh "
        "results."));
    return;
  }
  updateStateIcons();
  refreshPlan();
  setStatus(QStringLiteral("Profile options updated."));
}

void MainWindow::updateProfileMenu() {
  auto* menu = d_->profileButton->menu();
  menu->clear();
  connect(menu->addAction(QStringLiteral("Load...")), &QAction::triggered,
          this, &MainWindow::loadProfile);
  connect(menu->addAction(QStringLiteral("Save")), &QAction::triggered,
          this, &MainWindow::saveCurrentProfile);
  menu->addSeparator();
  const QStringList profiles = core::storage::listProfiles();
  if (profiles.isEmpty()) {
    auto* note = menu->addAction(QStringLiteral("No recent profiles"));
    note->setEnabled(false);
  } else {
    for (const QString& file : profiles) {
      const QString path = core::paths::profilesDirectory() + '/' + file;
      auto* action = menu->addAction(file);
      action->setCheckable(true);
      action->setChecked(path == d_->profilePath);
      action->setToolTip(path);
      connect(action, &QAction::triggered, this, [this, path]() {
        d_->profileLock.reset();
        auto lock = std::make_unique<core::storage::FileLock>(path, 0);
        if (lock->acquire()) {
          d_->profileLock = std::move(lock);
          d_->tempProfile = false;
          loadProfileFile(path);
        }
      });
    }
  }
  menu->addSeparator();
  connect(menu->addAction(QStringLiteral("Edit...")), &QAction::triggered,
          this, &MainWindow::editProfile);
  connect(menu->addAction(QStringLiteral("Save As...")), &QAction::triggered,
          this, &MainWindow::saveProfileAs);
  connect(menu->addAction(QStringLiteral("Open profiles directory")),
          &QAction::triggered, this, [this]() {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(core::paths::profilesDirectory()));
          });
  connect(menu->addAction(QStringLiteral("Reset")), &QAction::triggered,
          this, &MainWindow::resetProfile);
}

}  // namespace qt_rsync::ui
