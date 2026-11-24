#pragma once

#include <QIcon>
#include <QVector>
#include <QWidget>

#include <QPair>

namespace qt_rsync::ui {

// Small statistics strip lifted from the native-hybrid StatsWidget. Entries
// are text + colour; no icon generation dependency is needed yet.
class StatsBar : public QWidget {
  Q_OBJECT
 public:
  explicit StatsBar(QWidget* parent = nullptr);

  void setEntries(const QVector<QPair<QString, QString>>& entries);
  void setActionColours(const QVector<QString>& colours);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QVector<QPair<QString, QString>> entries_;
  QVector<QIcon> actionIcons_;
};

}  // namespace qt_rsync::ui
