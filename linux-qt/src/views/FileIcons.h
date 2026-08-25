#pragma once

#include <QColor>
#include <QIcon>

class QFileInfo;

// File-list icons drawn in the prism-fm style: a filled folder and an outlined
// page, tinted with the list's own foreground colours. Drawing them here rather
// than taking the desktop theme's icons keeps the list consistent with
// [colors] and identical across icon themes.
namespace tfx::views {

QIcon folderIcon(const QColor &color);
QIcon fileIcon(const QColor &color);

}
