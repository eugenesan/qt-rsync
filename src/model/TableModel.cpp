#include "model/TableModel.h"

namespace qt_rsync::model {

TableModel::TableModel(DataStore* store, Pane pane, QObject* parent)
    : QAbstractTableModel(parent), store_(store), pane_(pane) {}

int TableModel::rowCount(const QModelIndex&) const {
  return store_ ? store_->visible.size() : 0;
}
int TableModel::columnCount(const QModelIndex&) const { return 11; }
QVariant TableModel::data(const QModelIndex&, int) const { return {}; }
QVariant TableModel::headerData(int, Qt::Orientation, int) const { return {}; }
Qt::ItemFlags TableModel::flags(const QModelIndex&) const { return {}; }
bool TableModel::setData(const QModelIndex&, const QVariant&, int) { return false; }
void TableModel::refresh() { beginResetModel(); endResetModel(); }

}  // namespace qt_rsync::model
