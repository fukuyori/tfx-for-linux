#include "core/TabState.h"

#include "core/FileOperations.h"

#include <QtGlobal>

namespace tfx::core {

QString normalizedTabPath(const QString &path)
{
    return canonicalDirectoryPath(path);
}

QStringList normalizedTabPaths(const QStringList &paths)
{
    QStringList normalized;
    for (const QString &path : paths) {
        const QString canonical = normalizedTabPath(path);
        if (!canonical.isEmpty() && !normalized.contains(canonical)) {
            normalized.append(canonical);
        }
    }
    return normalized;
}

int clampedTabIndex(int index, int count)
{
    if (count <= 0) {
        return -1;
    }
    return qBound(0, index, count - 1);
}

}
