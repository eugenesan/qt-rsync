#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "core/Change.h"
#include "core/Decision.h"

namespace qt_rsync::model {

// Structure-of-arrays store. This is the shape that scaled to ~1M rows in the
// qpy-rsync native experiments; keep it as the single owned dataset. The UI
// models are views over this store and never own a second copy of decisions.
enum Action { Create = 0, Update = 1, Metadata = 2, Delete = 3, DestOnly = 4, NoChange = 5 };
enum DecisionValue { Inherit = 0, Include = 1, Exclude = 2, DeleteRequest = 3 };

class DataStore {
 public:
  void clear();
  void loadChanges(const QVector<core::Change>& changes);
  void addDirectories();
  void rebuildVisible();
  void setDecision(int id, int decision, bool subtree = false);
  int decisionForPath(const QString& path) const;
  QString pathOfId(int id) const;
  int rowOfPath(const QString& path) const;

  QVector<QString> paths;
  QVector<QString> names;
  QVector<int> actions;
  QVector<int> kinds;
  QVector<int> decisions;
  QVector<std::int64_t> sizes;
  QVector<QString> times;
  QVector<int> parents;
  QVector<int> depth;
  QVector<int> visible;
  QHash<QString, int> pathToId;
  QHash<int, int> rowOfId;
  QString rootSource;
  QString rootDestination;
  int sortColumn = 0;
  bool sortDescending = false;
};

}  // namespace qt_rsync::model
