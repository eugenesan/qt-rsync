#include "ui/LockedProfileDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace qt_rsync::ui {

namespace {
const char* kLockedMessage = "Still open in another instance.";
}

LockedResult promptLockedProfile(QWidget* parent, const QString& path,
                                 const QString& suggestedName) {
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Profile already open"));
  auto* layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(
      QStringLiteral("%1 is open in another instance.").arg(path), &dialog));
  auto* status = new QLabel(QString(), &dialog);
  status->setStyleSheet(QStringLiteral("color: #EA580C;"));
  layout->addWidget(status);
  layout->addWidget(new QLabel(QStringLiteral("Temporary profile name:"), &dialog));
  auto* name = new QLineEdit(suggestedName, &dialog);
  name->selectAll();
  layout->addWidget(name);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* useTemp = buttons->addButton(QStringLiteral("Use temporary profile"),
                                     QDialogButtonBox::ActionRole);
  auto* retry = buttons->addButton(QStringLiteral("Check again"),
                                   QDialogButtonBox::ActionRole);
  auto* quit = buttons->addButton(QStringLiteral("Quit without changes"),
                                  QDialogButtonBox::RejectRole);
  layout->addWidget(buttons);
  QObject::connect(useTemp, &QPushButton::clicked, &dialog, [&]() {
    dialog.done(static_cast<int>(LockedChoice::Temporary));
  });
  QObject::connect(quit, &QPushButton::clicked, &dialog, [&]() {
    dialog.done(static_cast<int>(LockedChoice::Quit));
  });

  LockedResult result;
  result.name = suggestedName;
  // Keep the dialog open on retry: running exec() only once also prevents the
  // nested-event-loop crashes seen with repeated dialogs.
  QObject::connect(retry, &QPushButton::clicked, &dialog, [&]() {
    auto lock = std::make_unique<core::storage::FileLock>(path, 0);
    if (lock->acquire()) {
      result.lock = std::move(lock);
      dialog.done(static_cast<int>(LockedChoice::UseOriginal));
    } else {
      status->setText(QString::fromLatin1(kLockedMessage));
    }
  });
  dialog.exec();
  result.choice = static_cast<LockedChoice>(dialog.result());
  result.name = name->text().trimmed();
  if (result.choice == LockedChoice::UseOriginal && !result.lock) {
    result.choice = LockedChoice::Quit;
  }
  return result;
}

}  // namespace qt_rsync::ui
