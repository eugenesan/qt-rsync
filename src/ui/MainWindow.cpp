#include "ui/MainWindow.h"

#include <QDialogButtonBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QMenu>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QTime>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTemporaryFile>
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
  QPushButton* compare = nullptr;
  QPushButton* verify = nullptr;
  QPushButton* sync = nullptr;
  QLabel* status = nullptr;
  QPlainTextEdit* journal = nullptr;
  QProgressBar* progress = nullptr;
  ComparisonView* comparison = nullptr;
  OverviewPane* overview = nullptr;
  QProcess* compareProcess = nullptr;
  QProcess* operationProcess = nullptr;
  QProcess* inventoryProcess = nullptr;
  QTemporaryFile* filterFile = nullptr;
  core::SyncProfile profile;
  QVector<core::Change> changes;
  core::ExecutionPlan plan;
  bool verified = false;
  bool stale = false;
  bool syncing = false;
  bool startupAborted = false;
  bool tempProfile = false;
  QString profilePath;
  std::unique_ptr<core::storage::FileLock> profileLock;
  QPushButton* profileButton = nullptr;
  QString logPath;
  QFile* logFile = nullptr;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), d_(new Private) {
  setWindowTitle(QStringLiteral("qt-rsync"));
  resize(1280, 800);

  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* layout = new QVBoxLayout(central);

  auto* paths = new QWidget(central);
  auto* form = new QFormLayout(paths);
  d_->source = new QLineEdit(paths);
  d_->destination = new QLineEdit(paths);
  d_->source->setPlaceholderText(QStringLiteral("/home/user/source"));
  d_->destination->setPlaceholderText(QStringLiteral("/home/user/destination"));
  form->addRow(QStringLiteral("Source:"), d_->source);
  form->addRow(QStringLiteral("Destination:"), d_->destination);

  auto* actions = new QWidget(paths);
  auto* actionLayout = new QHBoxLayout(actions);
  actionLayout->setContentsMargins(0, 0, 0, 0);
  d_->compare = new QPushButton(QStringLiteral("Compare"), actions);
  d_->verify = new QPushButton(QStringLiteral("Verify (F8)"), actions);
  d_->sync = new QPushButton(QStringLiteral("Synchronize (F9)"), actions);
  d_->verify->setEnabled(false);
  d_->sync->setEnabled(false);
  actionLayout->addWidget(d_->compare);
  actionLayout->addWidget(d_->verify);
  actionLayout->addWidget(d_->sync);
  form->addRow(QString(), actions);

  d_->profileButton = new QPushButton(QStringLiteral("Profile"), paths);
  d_->profileButton->setMenu(new QMenu(d_->profileButton));
  connect(d_->profileButton->menu(), &QMenu::aboutToShow,
          this, &MainWindow::updateProfileMenu);
  form->addRow(QStringLiteral("Profile:"), d_->profileButton);

  auto* settingsButton = new QPushButton(QStringLiteral("Settings..."), paths);
  connect(settingsButton, &QPushButton::clicked, this, [this]() {
    SettingsDialog dialog(this);
    dialog.exec();
  });
  form->addRow(QString(), settingsButton);
  layout->addWidget(paths);

  auto* splitter = new QSplitter(Qt::Vertical, central);
  d_->comparison = new ComparisonView(splitter);
  d_->overview = new OverviewPane(splitter);
  splitter->addWidget(d_->comparison);
  splitter->addWidget(d_->overview);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);

  d_->progress = new QProgressBar(central);
  d_->progress->setRange(0, 100);
  d_->progress->setValue(0);
  d_->progress->hide();
  layout->addWidget(d_->progress);
  d_->status = new QLabel(QStringLiteral("Ready"), central);
  layout->addWidget(d_->status);
  d_->journal = new QPlainTextEdit(central);
  d_->journal->setReadOnly(true);
  d_->journal->setMaximumHeight(140);
  layout->addWidget(d_->journal);

  d_->compareProcess = new QProcess(this);
  d_->operationProcess = new QProcess(this);
  d_->inventoryProcess = new QProcess(this);
  connect(d_->compare, &QPushButton::clicked, this, &MainWindow::startCompare);
  connect(d_->verify, &QPushButton::clicked, this, &MainWindow::verifyPlan);
  connect(d_->sync, &QPushButton::clicked, this, &MainWindow::synchronize);
  connect(d_->compareProcess, &QProcess::finished, this, &MainWindow::onCompareFinished);
  connect(d_->compareProcess, &QProcess::errorOccurred, this, &MainWindow::onCompareError);
  connect(d_->operationProcess, &QProcess::finished, this, &MainWindow::onOperationFinished);
  connect(d_->operationProcess, &QProcess::errorOccurred, this, &MainWindow::onOperationError);
  connect(d_->inventoryProcess, &QProcess::finished, this, &MainWindow::onInventoryFinished);
  connect(d_->operationProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    const QString text = core::cleanQtNoise(
        QString::fromLocal8Bit(d_->operationProcess->readAllStandardOutput()));
    for (const QString& line : text.split('\r')) {
      const QString clean = line.trimmed();
      if (clean.isEmpty()) continue;
      static const QRegularExpression percent(QStringLiteral("(\\d+)%"));
      const auto match = percent.match(clean);
      if (match.hasMatch()) {
        d_->progress->setValue(match.captured(1).toInt());
      }
      d_->journal->appendPlainText(clean);
    }
  });
  restoreProfile();

  connect(d_->comparison, &ComparisonView::decisionChanged, this, [this]() {
    d_->verified = false;
    refreshPlan();
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
}

MainWindow::~MainWindow() {
  discardTempProfile();
  closeSessionLog();
  delete d_;
}

void MainWindow::applyStartupOptions(const app::StartupOptions& options) {
  d_->source->setText(withTrailingSlash(options.source));
  d_->destination->setText(withTrailingSlash(options.destination));
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

void MainWindow::setStatus(const QString& text) {
  d_->status->setText(text);
  const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
  const QString line = time + QStringLiteral(" ") + text;
  d_->journal->appendPlainText(line);
  if (d_->logFile != nullptr && d_->logFile->isOpen()) {
    d_->logFile->write((line + '\n').toUtf8());
    d_->logFile->flush();
  }
}

void MainWindow::startCompare() {
  d_->source->setText(withTrailingSlash(d_->source->text()));
  d_->destination->setText(withTrailingSlash(d_->destination->text()));
  d_->profile.source.path = d_->source->text();
  d_->profile.destination.path = d_->destination->text();
  if (d_->profile.source.path.isEmpty() || d_->profile.destination.path.isEmpty()) {
    setStatus(QStringLiteral("Enter Source and Destination first."));
    return;
  }
  d_->profile.filters = core::filterTextToRules(d_->profile.filterText);
  const core::ProcessSpec spec = core::buildCompareSpec(d_->profile);
  setProcessEnvironment(spec, d_->compareProcess);
  d_->compare->setEnabled(false);
  setStatus(QStringLiteral("Comparing..."));
  d_->compareProcess->start(spec.program, spec.arguments);
}

void MainWindow::onCompareFinished(int exitCode, QProcess::ExitStatus status) {
  d_->compare->setEnabled(true);
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
  const auto baseName = [](const QString& path) {
    QString clean = path.trimmed();
    while (clean.endsWith('/')) clean.chop(1);
    const QString name = QFileInfo(clean).fileName();
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
  setStatus(QStringLiteral("Could not start rsync: %1")
                          .arg(d_->compareProcess->errorString()));
}

void MainWindow::startDestinationInventory() {
  const core::ProcessSpec spec =
      core::buildInventorySpec(d_->profile, d_->profile.destination);
  setProcessEnvironment(spec, d_->inventoryProcess);
  d_->inventoryProcess->start(spec.program, spec.arguments);
}

void MainWindow::onInventoryFinished(int, QProcess::ExitStatus) {
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
  d_->syncing = false;
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
  d_->syncing = true;
  d_->progress->show();
  d_->progress->setValue(0);
  setStatus(QStringLiteral("Synchronizing..."));
  d_->operationProcess->start(spec.program, spec.arguments);
}

void MainWindow::onOperationFinished(int exitCode, QProcess::ExitStatus status) {
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
  d_->verified = ok;
  setStatus(ok ? QStringLiteral("Verification matched the plan.")
                         : QStringLiteral("Verification failed: %1")
                               .arg(stderr.trimmed()));
}

void MainWindow::onOperationError(QProcess::ProcessError error) {
  Q_UNUSED(error)
  setStatus(QStringLiteral("Operation failed: %1")
                          .arg(d_->operationProcess->errorString()));
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
  d_->source->setText(d_->profile.source.path);
  d_->destination->setText(d_->profile.destination.path);
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
      d_->source->setText(d_->profile.source.path);
      d_->destination->setText(d_->profile.destination.path);
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
