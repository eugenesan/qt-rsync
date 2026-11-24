#include "ui/StatsBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>

#include "ui/Icons.h"

namespace qt_rsync::ui {
namespace {

const char* const actionLabels[] = {
    "Create", "Update", "Metadata only", "Delete", "Keep in Destination"};

}  // namespace

StatsBar::StatsBar(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(fontMetrics().height() + 10);
  layout_ = new QHBoxLayout(this);
  layout_->setContentsMargins(6, 4, 6, 4);
  layout_->setSpacing(4);
  summary_ = new QLabel(this);
  summary_->setObjectName(QStringLiteral("statistics-summary"));
  const QString summaryTip =
      QStringLiteral(
          "Selected: the items that are checked in the comparison,\n"
          "with their total size and the count of each action. The icons "
          "below are the same actions.");
  summary_->setToolTip(summaryTip);
  setToolTip(summaryTip);
  layout_->addWidget(summary_);
  layout_->addSpacing(8);
  for (int action = 0; action < 5; ++action) {
    auto* icon = new QLabel(this);
    icon->setToolTip(QString::fromLatin1(actionLabels[action]));
    auto* value = new QLabel(QStringLiteral("0"), this);
    value->setMinimumWidth(26);
    value->setToolTip(QString::fromLatin1(actionLabels[action]));
    layout_->addWidget(icon);
    layout_->addWidget(value);
    icons_.append(icon);
    values_.append(value);
  }
  layout_->addStretch(1);
}

void StatsBar::setEntries(const QVector<QPair<QString, QString>>& entries) {
  entries_ = entries;
  if (!entries_.isEmpty()) {
    const auto& summary = entries_.at(0);
    summary_->setText(summary.first);
    summary_->setStyleSheet(
        QStringLiteral("color: %1;").arg(summary.second));
  }
  for (int index = 1; index < entries_.size(); ++index) {
    const int action = index - 1;
    if (action < 0 || action >= values_.size()) continue;
    values_.at(action)->setText(entries_.at(index).first);
    values_.at(action)->setStyleSheet(
        QStringLiteral("color: %1;").arg(entries_.at(index).second));
  }
  if (actionIcons_.isEmpty()) {
    for (int action = 0; action < 5; ++action) {
      actionIcons_.append(icons::actionIcon(action));
    }
  }
  refreshIcons();
}

void StatsBar::setActionColours(const QVector<QString>& colours) {
  actionIcons_.clear();
  for (int action = 0; action < 5; ++action) {
    if (action < colours.size() && !colours.at(action).isEmpty()) {
      actionIcons_.append(
          icons::actionIcon(action, QColor(colours.at(action))));
    } else {
      actionIcons_.append(icons::actionIcon(action));
    }
  }
  refreshIcons();
}

void StatsBar::refreshIcons() {
  for (int action = 0; action < icons_.size() && action < actionIcons_.size();
       ++action) {
    icons_.at(action)->setPixmap(actionIcons_.at(action).pixmap(16, 16));
  }
}

}  // namespace qt_rsync::ui
