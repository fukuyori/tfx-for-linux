#include "core/GitService.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class GitServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void porcelainPathParsesRenameTargets();
    void porcelainAbsolutePathStaysInsideDirectory();
};

void GitServiceTest::porcelainPathParsesRenameTargets()
{
    QCOMPARE(tfx::core::porcelainPath("old.txt -> new.txt"), QString("new.txt"));
    QCOMPARE(tfx::core::porcelainPath("\"folder/file.txt\""), QString("folder/file.txt"));
}

void GitServiceTest::porcelainAbsolutePathStaysInsideDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir dir(temp.path());
    QVERIFY(dir.mkpath("folder"));
    QFile file(dir.filePath("folder/file.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QCOMPARE(tfx::core::porcelainAbsolutePath(temp.path(), "folder/file.txt"),
             QDir::cleanPath(dir.filePath("folder/file.txt")));
    QCOMPARE(tfx::core::porcelainAbsolutePath(temp.path(), "deleted.txt"),
             QDir::cleanPath(dir.filePath("deleted.txt")));

    QVERIFY(tfx::core::porcelainAbsolutePath(temp.path(), "/tmp/outside.txt").isEmpty());
    QVERIFY(tfx::core::porcelainAbsolutePath(temp.path(), "../outside.txt").isEmpty());
    QVERIFY(tfx::core::porcelainAbsolutePath(dir.filePath("missing"), "file.txt").isEmpty());
}

QTEST_GUILESS_MAIN(GitServiceTest)

#include "GitServiceTest.moc"
