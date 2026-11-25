#pragma once

#include <QRegularExpression>
#include <QString>

#include <optional>
#include <QStringList>
#include <QVector>

#include "core/Change.h"

namespace qt_rsync::core {

struct ParseResult {
  QVector<Change> changes;
  QStringList diagnostics;
};

// Derives progress from rsync's to-chk/ir-chk counters. Rsync's own
// percentage counts transferred bytes and stays at zero while it only lists
// the trees, so the counter is a more useful progress signal.
struct SyncProgress {
  int filesCompleted = 0;
  int filesRemaining = 0;
  int filesTotal = 0;
  int percent = 0;
};

std::optional<SyncProgress> parseSyncProgress(const QString& line);
bool isRsyncSummaryLine(const QString& line);

std::optional<Change> parseRecord(const QString& line, bool deleteEnabled = true);
ParseResult parseOutput(const QString& text, bool deleteEnabled = true);
QString describeItemized(const QString& code);
// Cheap structural check used for live progress counting. Accepts exactly the
// lines parseRecord() would turn into a change, without building a Change.
bool looksLikeItemizedRecord(const QString& rawLine);
QVector<Change> parseInventory(const QString& text);

}  // namespace qt_rsync::core
