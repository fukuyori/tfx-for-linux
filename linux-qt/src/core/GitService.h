#pragma once

#include <QString>

namespace tfx::core {

// Map a `git status --porcelain` two-character code to a single-letter badge
// (M/A/D/R/C/U/?/!).
QString porcelainStatusLabel(const QString &status);

// Extract the affected path from a `git status --porcelain` entry, handling the
// "old -> new" rename form and quoted paths.
QString porcelainPath(QString path);

// Resolve a porcelain relative path under a canonical Git query directory.
// Returns an empty string when the path is absolute or escapes the directory.
QString porcelainAbsolutePath(const QString &directory, const QString &relativePath);

}
