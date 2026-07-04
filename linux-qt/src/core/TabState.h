#pragma once

#include <QString>
#include <QStringList>

namespace tfx::core {

// Return the canonical directory path used for tab state, or empty for invalid
// entries.
QString normalizedTabPath(const QString &path);

// Normalize restored tab paths: keep only existing directories, canonicalize
// symlinks, and preserve the first occurrence of each directory.
QStringList normalizedTabPaths(const QStringList &paths);

int clampedTabIndex(int index, int count);

}
