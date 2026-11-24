#pragma once

#include <QIcon>
#include <QString>

namespace qt_rsync::ui::icons {

QIcon actionIcon(int action);
QIcon fileIcon(const QString& name);
QIcon folderIcon();
QIcon rootFolderIcon();
QIcon toolbarIcon(const QString& name);

}  // namespace qt_rsync::ui::icons
