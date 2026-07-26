#include "core/SidebarLogic.h"

#include <QtTest/QtTest>

using namespace tfx::core;

class SidebarLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void rootFilesystemIsAlwaysListed();
    void snapAndBootMountsAreSkipped();
    void blockDevicesAndNetworkMountsAreListed();
    void pseudoFilesystemsAreSkipped();
    void pinnedPathsAbbreviateHome();
};

void SidebarLogicTest::rootFilesystemIsAlwaysListed()
{
    QVERIFY(shouldListVolume("/", "/dev/nvme0n1p2", "ext4"));
    // Even on unusual devices (e.g. an overlay root), "/" stays visible.
    QVERIFY(shouldListVolume("/", "overlay", "overlay"));
}

void SidebarLogicTest::snapAndBootMountsAreSkipped()
{
    QVERIFY(!shouldListVolume("/snap/core/123", "/dev/loop3", "squashfs"));
    QVERIFY(!shouldListVolume("/boot/efi", "/dev/nvme0n1p1", "vfat"));
    QVERIFY(!shouldListVolume("/boot", "/dev/sda1", "ext4"));
    // A squashfs root path other than /snap is still noise.
    QVERIFY(!shouldListVolume("/media/image", "/dev/loop0", "squashfs"));
}

void SidebarLogicTest::blockDevicesAndNetworkMountsAreListed()
{
    QVERIFY(shouldListVolume("/home", "/dev/nvme0n1p3", "ext4"));
    QVERIFY(shouldListVolume("/media/usb", "/dev/sdb1", "vfat"));
    QVERIFY(shouldListVolume("/mnt/nas", "server:/export", "nfs4"));
    QVERIFY(shouldListVolume("/mnt/share", "//server/share", "cifs"));
    QVERIFY(shouldListVolume("/mnt/ssh", "user@host:", "fuse.sshfs"));
}

void SidebarLogicTest::pseudoFilesystemsAreSkipped()
{
    QVERIFY(!shouldListVolume("/tmp", "tmpfs", "tmpfs"));
    QVERIFY(!shouldListVolume("/run/user/1000", "tmpfs", "tmpfs"));
    QVERIFY(!shouldListVolume("/sys/fs/cgroup", "cgroup2", "cgroup2"));
    QVERIFY(!shouldListVolume("/snap/foo/1", "/dev/loop7", "squashfs"));
}

void SidebarLogicTest::pinnedPathsAbbreviateHome()
{
    const QString home = "/home/user";
    QCOMPARE(pinnedDisplayPath("/home/user", home), QString("~"));
    QCOMPARE(pinnedDisplayPath("/home/user/source/cpp/tfx", home), QString("~/source/cpp/tfx"));
    QCOMPARE(pinnedDisplayPath("/etc/nginx", home), QString("/etc/nginx"));
    // A sibling like /home/username2 must not be abbreviated.
    QCOMPARE(pinnedDisplayPath("/home/username2/docs", home), QString("/home/username2/docs"));
}

QTEST_GUILESS_MAIN(SidebarLogicTest)
#include "SidebarLogicTest.moc"
