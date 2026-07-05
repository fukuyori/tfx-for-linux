#pragma once

#include <QString>

namespace tfx::core {

// Recursively copy a file or directory tree. Symbolic links are recreated as
// links (preserving relative or absolute link text) instead of copying their
// targets. Returns false on the first failure.
bool copyRecursively(const QString &source, const QString &destination);

// Recreate the symbolic link at `source` as `destination`, preserving the raw
// link text. Returns false when `source` is not a link or the link cannot be
// created.
bool copySymbolicLink(const QString &source, const QString &destination);

// Copy the access and modification times of `source` onto `destination`.
// Symbolic links get the link's own times (not the target's). Creation time
// cannot be set on Linux and is left as-is.
bool copyTimestamps(const QString &source, const QString &destination);

// True when transferring `sourcePath` (a real directory) into
// `targetDirectory` would nest the directory inside itself. Both sides are
// canonicalized so the check cannot be bypassed through symlinked paths.
// Symbolic links to directories are copied as links, so they return false.
bool transferWouldNestInsideSource(const QString &sourcePath, const QString &targetDirectory);

// Return a path in `directory` based on `baseName` that does not yet exist,
// appending " 2", " 3", ... before the suffix when needed.
QString uniquePathInDirectory(const QString &directory, const QString &baseName);

// Return the canonical path for an existing directory, or an empty string when
// the input does not name an available directory.
QString canonicalDirectoryPath(const QString &path);

// Rename `oldName` to `newName` inside `directory`. A case-only rename
// (foo -> Foo) hops through a hidden temporary name, because a direct rename
// is refused or silently becomes a no-op on case-insensitive filesystems
// (exFAT/NTFS mounts). Rolls back on failure.
bool renameWithinDirectory(const QString &directory, const QString &oldName, const QString &newName);

}
