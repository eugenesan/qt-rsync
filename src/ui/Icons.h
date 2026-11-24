#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace qt_rsync::ui::icons {

QIcon actionIcon(int action);
QIcon actionIcon(int action, const QColor& colour);
QIcon badgedIcon(const QIcon& base, const QColor& colour);
QIcon fileIcon(const QString& name);
QIcon folderIcon();
QIcon rootFolderIcon();
QIcon toolbarIcon(const QString& name);

}  // namespace qt_rsync::ui::icons
