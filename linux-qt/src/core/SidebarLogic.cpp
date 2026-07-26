#include "core/SidebarLogic.h"

namespace tfx::core {

bool shouldListVolume(const QString &rootPath, const QString &device, const QString &fileSystemType)
{
    if (fileSystemType == QLatin1String("squashfs") || rootPath.startsWith(QLatin1String("/boot"))) {
        return false;
    }
    if (rootPath == QLatin1String("/")) {
        return true;
    }
    const bool physical = device.startsWith(QLatin1String("/dev/"))
        && !device.startsWith(QLatin1String("/dev/loop"));
    const bool network = fileSystemType.startsWith(QLatin1String("nfs"))
        || fileSystemType == QLatin1String("cifs")
        || fileSystemType.startsWith(QLatin1String("smb"))
        || fileSystemType == QLatin1String("fuse.sshfs");
    return physical || network;
}

QString pinnedDisplayPath(const QString &path, const QString &home)
{
    if (path == home) {
        return QStringLiteral("~");
    }
    if (path.startsWith(home + QLatin1Char('/'))) {
        return QStringLiteral("~") + path.mid(home.size());
    }
    return path;
}

}
