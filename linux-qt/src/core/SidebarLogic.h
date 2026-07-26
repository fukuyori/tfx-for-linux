#pragma once

#include <QString>

namespace tfx::core {

// True when a mounted volume belongs in the DISKS sidebar section: the root
// filesystem, real block devices (loop-mounted squashfs like snaps and /boot
// partitions are noise), and network mounts.
bool shouldListVolume(const QString &rootPath, const QString &device, const QString &fileSystemType);

// Pinned-row display text: the folder with its directory, where paths under
// `home` start with "~" (home itself is "~").
QString pinnedDisplayPath(const QString &path, const QString &home);

}
