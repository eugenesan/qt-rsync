#include "ui/MainWindow.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "core/Command.h"
#include "core/Parser.h"
#include "ui/ComparisonView.h"
#include "ui/OverviewPane.h"

namespace qt_rsync::ui {

class MainWindow::Private {
 public:
  QLineEdit* source = nullptr;
  QLineEdit* destination = nullptr;
  QPushButton* compare = nullptr;
  QLabel* status = nullptr;
  ComparisonView* comparison = nullptr;
  OverviewPane* overview = nullptr;
  QProcess* process = nullptr;
  core::SyncProfile profile;
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
  d_->compare = new QPushButton(QStringLiteral("Compare"), paths);
  form->addRow(QString(), d_->compare);
  layout->addWidget(paths);

  auto* splitter = new QSplitter(Qt::Vertical, central);
  d_->comparison = new ComparisonView(splitter);
  d_->overview = new OverviewPane(splitter);
  splitter->addWidget(d_->comparison);
  splitter->addWidget(d_->overview);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);

  d_->status = new QLabel(QStringLiteral("Ready"), central);
  layout->addWidget(d_->status);

  connect(d_->comparison, &ComparisonView::decisionChanged,
          this, [this]() { d_->overview->rebuild(d_->comparison->store()); });

  d_->process = new QProcess(this);
  connect(d_->compare, &QPushButton::clicked, this, &MainWindow::startCompare);
  connect(d_->process, &QProcess::finished, this, &MainWindow::onCompareFinished);
  connect(d_->process, &QProcess::errorOccurred, this, &MainWindow::onCompareError);
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

void MainWindow::startCompare() {
  d_->profile.source.path = d_->source->text();
  d_->profile.destination.path = d_->destination->text();
  if (d_->profile.source.path.isEmpty() || d_->profile.destination.path.isEmpty()) {
    d_->status->setText(QStringLiteral("Enter Source and Destination first."));
    return;
  }

  const core::ProcessSpec spec = core::buildCompareSpec(d_->profile);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  for (auto it = spec.environment.cbegin(); it != spec.environment.cend(); ++it) {
    environment.insert(it.key(), it.value());
  }
  d_->process->setProcessEnvironment(environment);
  d_->compare->setEnabled(false);
  d_->status->setText(QStringLiteral("Comparing..."));
  d_->process->start(spec.program, spec.arguments);
}

void MainWindow::onCompareFinished(int exitCode, QProcess::ExitStatus status) {
  d_->compare->setEnabled(true);
  const QByteArray output = d_->process->readAllStandardOutput();
  const QString stderr = QString::fromLocal8Bit(d_->process->readAllStandardError());
  if (status != QProcess::NormalExit || exitCode != 0) {
    d_->status->setText(QStringLiteral("Compare failed: %1")
                            .arg(stderr.trimmed().isEmpty()
                                     ? QString::number(exitCode)
                                     : stderr.trimmed()));
    return;
  }
  const core::ParseResult parsed = core::parseOutput(
      QString::fromUtf8(output), d_->profile.transfer.mirrorEnabled());
  d_->comparison->setChanges(parsed.changes);
  d_->overview->rebuild(d_->comparison->store());
  d_->status->setText(QStringLiteral("Comparison complete: %1 item(s)")
                          .arg(parsed.changes.size()));
}

void MainWindow::onCompareError(QProcess::ProcessError error) {
  d_->compare->setEnabled(true);
  Q_UNUSED(error)
  d_->status->setText(QStringLiteral("Could not start rsync: %1")
                          .arg(d_->process->errorString()));
}

}  // namespace qt_rsync::ui
