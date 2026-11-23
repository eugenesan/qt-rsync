#include "ui/MainWindow.h"

#include <QDialogButtonBox>
#include <QFormLayout>
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
#include "core/Storage.h"
#include "ui/ComparisonView.h"
#include "ui/OverviewPane.h"
#include "ui/SettingsDialog.h"
#include "ui/SyncDialog.h"

namespace qt_rsync::ui {
namespace {

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
  QTemporaryFile* filterFile = nullptr;
  core::SyncProfile profile;
  QVector<core::Change> changes;
  core::ExecutionPlan plan;
  bool verified = false;
  bool stale = false;
  bool syncing = false;
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
  connect(d_->compare, &QPushButton::clicked, this, &MainWindow::startCompare);
  connect(d_->verify, &QPushButton::clicked, this, &MainWindow::verifyPlan);
  connect(d_->sync, &QPushButton::clicked, this, &MainWindow::synchronize);
  connect(d_->compareProcess, &QProcess::finished, this, &MainWindow::onCompareFinished);
  connect(d_->compareProcess, &QProcess::errorOccurred, this, &MainWindow::onCompareError);
  connect(d_->operationProcess, &QProcess::finished, this, &MainWindow::onOperationFinished);
  connect(d_->operationProcess, &QProcess::errorOccurred, this, &MainWindow::onOperationError);
  connect(d_->operationProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    const QString text = core::cleanQtNoise(
        QString::fromLocal8Bit(d_->operationProcess->readAllStandardOutput()));
    for (const QString& line : text.split('\r')) {
      const QString clean = line.trimmed();
      if (clean.isEmpty()) continue;
      static const QRegularExpression percent(QStringLiteral("(\d+)%"));
      const auto match = percent.match(clean);
      if (match.hasMatch()) {
        d_->progress->setValue(match.captured(1).toInt());
      }
      d_->journal->appendPlainText(clean);
    }
  });
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

MainWindow::~MainWindow() { delete d_; }

void MainWindow::applyStartupOptions(const app::StartupOptions& options) {
  d_->source->setText(options.source);
  d_->destination->setText(options.destination);
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
  d_->journal->appendPlainText(time + QStringLiteral(" ") + text);
}

void MainWindow::startCompare() {
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
  d_->comparison->setChanges(parsed.changes);
  d_->overview->rebuild(d_->comparison->store());
  refreshPlan();
  d_->stale = false;
  setStatus(QStringLiteral("Comparison complete: %1 item(s)")
                          .arg(parsed.changes.size()));
}

void MainWindow::onCompareError(QProcess::ProcessError error) {
  Q_UNUSED(error)
  d_->compare->setEnabled(true);
  setStatus(QStringLiteral("Could not start rsync: %1")
                          .arg(d_->compareProcess->errorString()));
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

}  // namespace qt_rsync::ui
