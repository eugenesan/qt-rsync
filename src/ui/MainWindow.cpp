#include "ui/MainWindow.h"

#include <QVBoxLayout>
#include <QWidget>

#include "ui/ComparisonView.h"
#include "ui/OverviewPane.h"

namespace qt_rsync::ui {

class MainWindow::Private {
 public:
  ComparisonView* comparison = nullptr;
  OverviewPane* overview = nullptr;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), d_(new Private) {
  setWindowTitle(QStringLiteral("qt-rsync"));
  resize(1280, 800);
  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* layout = new QVBoxLayout(central);
  d_->comparison = new ComparisonView(central);
  d_->overview = new OverviewPane(central);
  layout->addWidget(d_->comparison, 3);
  layout->addWidget(d_->overview, 1);
}

MainWindow::~MainWindow() { delete d_; }

void MainWindow::applyStartupOptions(const app::StartupOptions&) {
  // Phase 2: populate endpoints, mode and backup from the parsed options.
}

}  // namespace qt_rsync::ui
