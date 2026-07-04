#pragma once

#include <QString>

namespace tfx::core {

// Recursively copy a file or directory tree. Returns false on the first failure.
bool copyRecursively(const QString &source, const QString &destination);

// Return a path in `directory` based on `baseName` that does not yet exist,
// appending " 2", " 3", ... before the suffix when needed.
QString uniquePathInDirectory(const QString &directory, const QString &baseName);

// Return the canonical path for an existing directory, or an empty string when
// the input does not name an available directory.
QString canonicalDirectoryPath(const QString &path);

}
