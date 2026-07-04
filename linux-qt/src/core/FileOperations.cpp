#include "core/FileOperations.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace tfx::core {

bool copyRecursively(const QString &source, const QString &destination)
{
    const QFileInfo sourceInfo(source);
    if (sourceInfo.isDir()) {
        QDir destinationDir(destination);
        if (!destinationDir.exists() && !QDir().mkpath(destination)) {
            return false;
        }

        QDirIterator iterator(source, QDir::NoDotAndDotDot | QDir::AllEntries);
        while (iterator.hasNext()) {
            iterator.next();
            const QString childSource = iterator.filePath();
            const QString childDestination = QDir(destination).filePath(iterator.fileName());
            if (!copyRecursively(childSource, childDestination)) {
                return false;
            }
        }
        return true;
    }

    return QFile::copy(source, destination);
}

QString uniquePathInDirectory(const QString &directory, const QString &baseName)
{
    const QString first = QDir(directory).filePath(baseName);
    if (!QFileInfo::exists(first)) {
        return first;
    }

    const QFileInfo info(first);
    for (int index = 2; ; ++index) {
        const QString name = info.suffix().isEmpty()
            ? QString("%1 %2").arg(info.completeBaseName()).arg(index)
            : QString("%1 %2.%3").arg(info.completeBaseName()).arg(index).arg(info.suffix());
        const QString candidate = QDir(directory).filePath(name);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
}

QString canonicalDirectoryPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        return QString();
    }
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

}
