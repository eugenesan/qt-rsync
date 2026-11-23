#pragma once

#include <QAbstractItemModel>

#include "model/DataStore.h"

namespace qt_rsync::model {

class OverviewModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  explicit OverviewModel(DataStore* store, QObject* parent = nullptr);

  QModelIndex index(int row, int column,
                    const QModelIndex& parent = {}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  void rebuild();

 private:
  DataStore* store_;
};

}  // namespace qt_rsync::model
