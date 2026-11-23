#pragma once

#include <QMainWindow>
#include <QProcess>

#include "app/StartupOptions.h"
#include "core/Change.h"
#include "core/Plan.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTemporaryFile;

namespace qt_rsync::ui {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void applyStartupOptions(const app::StartupOptions& options);

 private slots:
  void startCompare();
  void onCompareFinished(int exitCode, QProcess::ExitStatus status);
  void onCompareError(QProcess::ProcessError error);
  void verifyPlan();
  void synchronize();
  void onOperationFinished(int exitCode, QProcess::ExitStatus status);
  void onOperationError(QProcess::ProcessError error);
  void refreshPlan();
  void addFilterPattern(const QString& pattern, const QString& path);

 private:
  class Private;
  Private* d_;
};

}  // namespace qt_rsync::ui
