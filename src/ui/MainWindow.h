#pragma once

#include <QMainWindow>
#include <QProcess>

#include "app/StartupOptions.h"

class QLabel;
class QLineEdit;
class QPushButton;

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

 private:
  class Private;
  Private* d_;
};

}  // namespace qt_rsync::ui
