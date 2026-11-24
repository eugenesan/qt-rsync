#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/Change.h"

namespace qt_rsync::core {

struct ParseResult {
  QVector<Change> changes;
  QStringList diagnostics;
};

std::optional<Change> parseRecord(const QString& line, bool deleteEnabled = true);
ParseResult parseOutput(const QString& text, bool deleteEnabled = true);
QString describeItemized(const QString& code);
QVector<Change> parseInventory(const QString& text);

}  // namespace qt_rsync::core
