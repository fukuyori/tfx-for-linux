#pragma once

#include <QColor>
#include <QIcon>

class QFileInfo;

// File-category icons inspired by prism-fm, drawn with Qt and tinted with
// the list foreground colours. Shared by normal, search, archive, and trash
// views; independent of the desktop icon theme.
namespace tfx::views {

enum class FileIconKind { File, Folder, Image, Video, Audio, Pdf, Spreadsheet, Presentation, Document, Text, Code, Markup, Data, Script, Database, Font, Archive, Disk, Executable, Library, Key, Threed };
FileIconKind fileIconKind(const QString &name);
QIcon fileIcon(const QString &name, const QColor &color);

QIcon folderIcon(const QColor &color);
QIcon fileIcon(const QColor &color);

}
