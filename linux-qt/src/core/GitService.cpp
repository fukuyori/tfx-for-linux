#include "core/GitService.h"

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

}
