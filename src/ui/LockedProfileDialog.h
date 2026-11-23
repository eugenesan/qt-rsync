#pragma once

#include <QString>

#include <memory>

#include "core/Storage.h"

namespace qt_rsync::ui {

enum class LockedChoice { Temporary, UseOriginal, Quit };

struct LockedResult {
  LockedChoice choice = LockedChoice::Quit;
  QString name;
  std::unique_ptr<core::storage::FileLock> lock;
};

// Asks what to do when a profile is held by another process. "Check again"
// retries inside the same dialog and turns the message orange while locked.
LockedResult promptLockedProfile(QWidget* parent, const QString& path,
                                 const QString& suggestedName);

}  // namespace qt_rsync::ui
