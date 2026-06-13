#pragma once

#include <QString>

class QFileInfo;

namespace tfx::core {

// Platform-agnostic, English file-type and size/mode formatting used by the
// file list and search results.
QString englishTypeName(const QFileInfo &info);
QString sizeString(qint64 bytes);
QString modeString(const QFileInfo &info);

}
