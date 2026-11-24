#pragma once

#include <QIcon>
#include <QVector>
#include <QWidget>

#include <QPair>

class QHBoxLayout;
class QLabel;

namespace qt_rsync::ui {

// One-line statistics strip for the overview. Labels rather than painted
// text, so long entries cannot be clipped and each action keeps a tooltip.
class StatsBar : public QWidget {
  Q_OBJECT
 public:
  explicit StatsBar(QWidget* parent = nullptr);

  void setEntries(const QVector<QPair<QString, QString>>& entries);
  void setActionColours(const QVector<QString>& colours);

 private:
  void refreshIcons();

  QHBoxLayout* layout_ = nullptr;
  QLabel* summary_ = nullptr;
  QVector<QLabel*> icons_;
  QVector<QLabel*> values_;
  QVector<QPair<QString, QString>> entries_;
  QVector<QIcon> actionIcons_;
};

}  // namespace qt_rsync::ui
