#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include "core/Profile.h"

namespace qt_rsync::core {

struct ToolContext {
  QString pickedFullPath;
  QString pickedDirectory;
  QString pickedName;
  QString otherFullPath;
  QString otherDirectory;
  QString otherName;
  Endpoint pickedEndpoint;
  Endpoint otherEndpoint;
  QString toolName;
  QString options;
};

ToolContext itemContext(const QString& path, bool isDirectory,
                        const QString& pickedRoot, const QString& otherRoot,
                        const Endpoint& pickedEndpoint,
                        const Endpoint& otherEndpoint,
                        const QString& toolName = QString(),
                        const QString& options = QString());

QHash<QString, QString> placeholderValues(const ToolContext& context);
QString expandPlaceholders(const QString& text,
                           const QHash<QString, QString>& values);
QStringList unknownPlaceholders(const QString& text);

// Splits and expands a tool command into an argument vector. Errors (empty
// command or unbalanced quotes) are returned in `error`.
QStringList buildToolCommand(const QString& command,
                             const QHash<QString, QString>& values,
                             QString* error = nullptr);

QString joinToolPath(const QString& root, const QString& path);

}  // namespace qt_rsync::core
