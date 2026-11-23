#pragma once

#include <QMainWindow>

#include "app/StartupOptions.h"

namespace qt_rsync::ui {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void applyStartupOptions(const app::StartupOptions& options);

 private:
  class Private;
  Private* d_;
};

}  // namespace qt_rsync::ui
