#include "core/Trash.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <unistd.h>

using namespace tfx::core;

class TrashTest : public QObject
{
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }
    static TrashLocation location(const QTemporaryDir &tmp)
    {
        const QString path = tmp.path() + "/Trash";
        QDir().mkpath(path + "/files");
        QDir().mkpath(path + "/info");
        for (const QString &dir : {path, path + "/files", path + "/info"})
            QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return {path, tmp.path()};
    }
    static bool info(const TrashLocation &root, const QString &id, const QString &original)
    {
        return write(root.path + "/info/" + id + ".trashinfo", "[Trash Info]\nPath="
            + QFile::encodeName(original).toPercentEncoding("/")
            + "\nDeletionDate=2026-09-09T12:34:56\n");
    }

private slots:
    void deletesDirectoriesWithoutFollowingSymlinks()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        QVERIFY(QDir().mkpath(root.path + "/files/dir/nested"));
        QVERIFY(write(root.path + "/files/dir/nested/file", "trash"));
        const QString outside = tmp.path() + "/keep";
        QVERIFY(write(outside, "keep"));
        QCOMPARE(::symlink(QFile::encodeName(outside).constData(),
                           QFile::encodeName(root.path + "/files/dir/link").constData()), 0);
        QCOMPARE(::symlink(QFile::encodeName(outside).constData(),
                           QFile::encodeName(root.path + "/files/link").constData()), 0);
        QVERIFY(write(root.path + "/files/unselected", "keep"));
        QVERIFY(info(root, "dir", "original-dir"));
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 3);
        QString error;
        for (const auto &entry : list.entries) {
            if (entry.id == "unselected") continue;
            QVERIFY2(deleteTrashEntry(entry, &error), qPrintable(error));
            QVERIFY(error.isEmpty());
        }
        QVERIFY(QFileInfo::exists(outside));
        QVERIFY(QFileInfo::exists(root.path + "/files/unselected"));
        QVERIFY(!QFileInfo::exists(root.path + "/files/dir"));
        QVERIFY(!QFileInfo(root.path + "/files/link").isSymLink());
        QVERIFY(!QFileInfo::exists(root.path + "/info/dir.trashinfo"));
    }

    void deletionRejectsPathsOutsideTrash()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        QVERIFY(write(root.path + "/keep", "keep"));
        QString error;
        QVERIFY(!deleteTrashEntry({root, "../keep", {}, {}, {}}, &error));
        QVERIFY(QFileInfo::exists(root.path + "/keep"));
        QVERIFY(!deleteTrashEntry({root, "..", {}, {}, {}}, &error));
    }

    void restoresEncodedOriginalName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto root = location(tmp);
        const QString original = tmp.path() + "/日本語 % name.txt";
        QVERIFY(write(root.path + "/files/internal-id", "contents"));
        QVERIFY(info(root, "internal-id", original));
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 1);
        QCOMPARE(list.entries.first().originalPath, original);
        QVERIFY(list.entries.first().deletionDate.isValid());
        QString error;
        QVERIFY2(restoreTrashEntry(list.entries.first(), &error), qPrintable(error));
        QFile restored(original);
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), QByteArray("contents"));
        QVERIFY(!QFileInfo::exists(root.path + "/info/internal-id.trashinfo"));
        QVERIFY(listTrash({root}).entries.isEmpty());
    }

    void collisionsPreserveBothItems_data()
    {
        QTest::addColumn<bool>("symlink");
        QTest::newRow("existing-file") << false;
        QTest::newRow("dangling-symlink") << true;
    }
    void collisionsPreserveBothItems()
    {
        QFETCH(bool, symlink);
        QTemporaryDir tmp;
        const auto root = location(tmp);
        const QString original = tmp.path() + "/existing";
        if (symlink)
            QCOMPARE(::symlink("missing-target", QFile::encodeName(original).constData()), 0);
        else
            QVERIFY(write(original, "existing contents"));
        QVERIFY(write(root.path + "/files/item", "trashed contents"));
        QVERIFY(info(root, "item", original));
        QString error;
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 1);
        QVERIFY(!restoreTrashEntry(list.entries.first(), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(QFileInfo::exists(root.path + "/files/item"));
        QVERIFY(QFileInfo::exists(root.path + "/info/item.trashinfo"));
        if (symlink) {
            QVERIFY(QFileInfo(original).isSymLink());
        } else {
            QFile file(original);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("existing contents"));
        }
    }

    void restoresDirectoriesAndDanglingSymlinks()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        QVERIFY(QDir().mkpath(root.path + "/files/dir"));
        QVERIFY(write(root.path + "/files/dir/child", "child"));
        QCOMPARE(::symlink("missing-target", QFile::encodeName(root.path + "/files/link").constData()), 0);
        QVERIFY(info(root, "dir", "restored-dir"));
        QVERIFY(info(root, "link", "restored-link"));
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 2);
        QString error;
        for (const auto &entry : list.entries)
            QVERIFY2(restoreTrashEntry(entry, &error), qPrintable(error));
        QVERIFY(QFileInfo::exists(tmp.path() + "/restored-dir/child"));
        QVERIFY(QFileInfo(tmp.path() + "/restored-link").isSymLink());
    }

    void invalidMetadataIsVisibleAndCannotBeRestored()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        QVERIFY(write(root.path + "/files/no-info", "data"));
        QVERIFY(write(root.path + "/files/traversal", "data"));
        QVERIFY(info(root, "traversal", "../outside"));
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 2);
        for (const auto &entry : list.entries) {
            QVERIFY(!entry.error.isEmpty());
            QString error;
            QVERIFY(!restoreTrashEntry(entry, &error));
            QVERIFY(QFileInfo::exists(root.path + "/files/" + entry.id));
        }
    }

    void missingParentAndChangedMetadataPreserveTrash()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        QVERIFY(write(root.path + "/files/item", "data"));
        QVERIFY(info(root, "item", "missing-parent/file"));
        const auto list = listTrash({root});
        QCOMPARE(list.entries.size(), 1);
        const auto entry = list.entries.first();
        QString error;
        QVERIFY(!restoreTrashEntry(entry, &error));
        QVERIFY(info(root, "item", "changed"));
        QVERIFY(!restoreTrashEntry(entry, &error));
        QVERIFY(QFileInfo::exists(root.path + "/files/item"));
        QVERIFY(!QFileInfo::exists(tmp.path() + "/changed"));
    }

    void symlinkedTrashDirectoryIsRejected()
    {
        QTemporaryDir tmp;
        const auto root = location(tmp);
        const QString link = tmp.path() + "/linked-trash";
        QCOMPARE(::symlink(QFile::encodeName(root.path).constData(), QFile::encodeName(link).constData()), 0);
        const auto list = listTrash({{link, tmp.path()}});
        QVERIFY(list.entries.isEmpty());
        QVERIFY(!list.errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TrashTest)
#include "TrashTest.moc"
