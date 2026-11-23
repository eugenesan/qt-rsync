#pragma once

#include <QAbstractTableModel>

#include "model/DataStore.h"

namespace qt_rsync::model {

class TableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  enum Pane { Source = 0, Action = 1, Destination = 2 };
  Q_ENUM(Pane)

  explicit TableModel(DataStore* store, Pane pane, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole) override;

  void refresh();

 private:
  DataStore* store_;
  Pane pane_;
};

}  // namespace qt_rsync::model
