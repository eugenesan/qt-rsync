#include "model/DataStore.h"

namespace qt_rsync::model {

void DataStore::clear() { *this = DataStore{}; }
void DataStore::loadChanges(const QVector<core::Change>&) {}
void DataStore::addDirectories() {}
void DataStore::rebuildVisible() {}
void DataStore::setDecision(int, int, bool) {}
int DataStore::decisionForPath(const QString&) const { return Inherit; }
QString DataStore::pathOfId(int) const { return {}; }
int DataStore::rowOfPath(const QString&) const { return -1; }

}  // namespace qt_rsync::model
