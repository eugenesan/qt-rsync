#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>

#include "app/StartupOptions.h"
#include "core/Change.h"
#include "core/Command.h"
#include "core/Plan.h"
#include "core/Storage.h"
#include "ui/ActionFilterBar.h"

#include <memory>

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
  [[nodiscard]] bool startupAborted() const;

 private slots:
  void startCompare();
  void onCompareFinished(int exitCode, QProcess::ExitStatus status);
  void onCompareError(QProcess::ProcessError error);
  void startDestinationInventory();
  void onInventoryFinished(int exitCode, QProcess::ExitStatus status);
  void onDestinationInventoryFinished(int exitCode, QProcess::ExitStatus status);
  void startInventory();
  void verifyPlan();
  void synchronize();
  void onOperationFinished(int exitCode, QProcess::ExitStatus status);
  void onOperationError(QProcess::ProcessError error);
  void refreshPlan();
  void addFilterPattern(const QString& pattern, const QString& path);
  void setStatus(const QString& text);
  void appendJournalLine(const QString& line);
  void setCommandPreview(const core::ProcessSpec& spec);
  void cancelOperation();
  void setProfilePath(const QString& path);
  void openSessionLog(const QString& name);
  void closeSessionLog();
  void promoteTempProfile(const QString& newName);
  void discardTempProfile();
  void restoreProfile();
  void loadProfileFile(const QString& path);
  void saveProfileAs();
  void saveCurrentProfile();
  void loadProfile();
  void resetProfile();
  void updateProfileMenu();
  void editProfile();
  void findNext();
  void findPrevious();

 private:
  void refreshSearchMatches();
  void stepSearch(int direction);
  void applyActionColours();
  void updateVerificationVisibility();
  void updateStateIcons();
  void applyLayoutMode(const QString& mode);
  void filterUnchangedAgainstDestination();
  void mergeInventoryChanges();

 private:
  class Private;
  Private* d_;
};

}  // namespace qt_rsync::ui
