#pragma once

#include <QDialog>

#include "core/Command.h"
#include "core/Plan.h"

namespace qt_rsync::ui {

// Confirmation dialog: command preview, counts and the temporary decision
// rules. It never starts rsync itself; the caller owns the process.
class SyncDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SyncDialog(const core::SyncProfile& profile,
                      const core::ExecutionPlan& plan,
                      const QString& filterFilePath,
                      QWidget* parent = nullptr);
};

}  // namespace qt_rsync::ui
