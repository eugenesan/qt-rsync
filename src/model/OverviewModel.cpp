#include "model/OverviewModel.h"

namespace qt_rsync::model {

OverviewModel::OverviewModel(DataStore* store, QObject* parent)
    : QAbstractItemModel(parent), store_(store) {}

QModelIndex OverviewModel::index(int, int, const QModelIndex&) const { return {}; }
QModelIndex OverviewModel::parent(const QModelIndex&) const { return {}; }
int OverviewModel::rowCount(const QModelIndex&) const { return 0; }
int OverviewModel::columnCount(const QModelIndex&) const { return 1; }
QVariant OverviewModel::data(const QModelIndex&, int) const { return {}; }
QVariant OverviewModel::headerData(int, Qt::Orientation, int) const { return {}; }
void OverviewModel::rebuild() { beginResetModel(); endResetModel(); }

}  // namespace qt_rsync::model
