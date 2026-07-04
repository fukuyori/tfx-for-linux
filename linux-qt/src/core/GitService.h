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

// Convert a repository-root-relative porcelain path to one relative to the
// queried directory, given that directory's prefix inside the repository (as
// printed by `git rev-parse --show-prefix`, e.g. "sub/dir/" or empty at the
// root). Returns an empty string when the path lies outside the prefix.
QString porcelainPathInDirectory(const QString &path, const QString &prefix);

}
