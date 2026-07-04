#include "core/GitService.h"

#include <QDir>
#include <QFileInfo>

namespace tfx::core {

QString porcelainStatusLabel(const QString &status)
{
    if (status.contains("??")) {
        return "?";
    }
    if (status.contains("A")) {
        return "A";
    }
    if (status.contains("D")) {
        return "D";
    }
    if (status.contains("R")) {
        return "R";
    }
    if (status.contains("C")) {
        return "C";
    }
    if (status.contains("U")) {
        return "U";
    }
    if (status.contains("M")) {
        return "M";
    }
    if (status.contains("!")) {
        return "!";
    }
    return status.trimmed();
}

QString porcelainPath(QString path)
{
    path = path.trimmed();
    const int renameArrow = path.indexOf(" -> ");
    if (renameArrow >= 0) {
        path = path.mid(renameArrow + 4);
    }
    if (path.size() >= 2 && path.startsWith('"') && path.endsWith('"')) {
        path = path.mid(1, path.size() - 2);
    }
    return path;
}

QString porcelainAbsolutePath(const QString &directory, const QString &relativePath)
{
    const QString basePath = QFileInfo(directory).canonicalFilePath();
    if (basePath.isEmpty() || relativePath.isEmpty()) {
        return QString();
    }

    const QString normalizedRelative = QDir::fromNativeSeparators(relativePath);
    if (QDir::isAbsolutePath(normalizedRelative)) {
        return QString();
    }

    const QString cleanRelative = QDir::cleanPath(normalizedRelative);
    if (cleanRelative == "." || cleanRelative == ".." || cleanRelative.startsWith("../")) {
        return QString();
    }

    const QString absolutePath = QDir::cleanPath(QDir(basePath).filePath(cleanRelative));
    if (absolutePath == basePath || absolutePath.startsWith(basePath + "/")) {
        return absolutePath;
    }
    return QString();
}

QString porcelainPathInDirectory(const QString &path, const QString &prefix)
{
    if (prefix.isEmpty()) {
        return path;
    }
    if (!path.startsWith(prefix)) {
        return QString();
    }
    return path.mid(prefix.size());
}

}
