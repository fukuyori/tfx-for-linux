#include "core/FileOperations.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool writeTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << "x";
    return true;
}
}

class PathHandlingTest : public QObject
{
    Q_OBJECT

private slots:
    void canonicalDirectoryPathAcceptsOnlyExistingDirectories();
    void canonicalDirectoryPathResolvesAliasesAndTrailingSeparators();
    void uniquePathInDirectoryAddsNumericSuffixes();
    void copyRecursivelyCopiesNestedDirectories();
    void copyRecursivelyFailsWhenDestinationExists();
};

void PathHandlingTest::canonicalDirectoryPathAcceptsOnlyExistingDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString directory = QDir(temp.path()).filePath("folder");
    const QString file = QDir(temp.path()).filePath("file.txt");
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(writeTextFile(file));

    QCOMPARE(tfx::core::canonicalDirectoryPath(directory), QFileInfo(directory).canonicalFilePath());
    QVERIFY(tfx::core::canonicalDirectoryPath(file).isEmpty());
    QVERIFY(tfx::core::canonicalDirectoryPath(QDir(temp.path()).filePath("missing")).isEmpty());
}

void PathHandlingTest::canonicalDirectoryPathResolvesAliasesAndTrailingSeparators()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString directory = root.filePath("folder");
    QVERIFY(QDir().mkpath(directory));

    QCOMPARE(tfx::core::canonicalDirectoryPath(directory + QDir::separator()),
             QFileInfo(directory).canonicalFilePath());

    const QString alias = root.filePath("folder-alias");
    QFile::link(directory, alias);
    if (QFileInfo(alias).exists()) {
        QCOMPARE(tfx::core::canonicalDirectoryPath(alias), QFileInfo(directory).canonicalFilePath());
    }
}

void PathHandlingTest::uniquePathInDirectoryAddsNumericSuffixes()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QVERIFY(writeTextFile(QDir(temp.path()).filePath("report.txt")));
    QVERIFY(writeTextFile(QDir(temp.path()).filePath("report 2.txt")));

    QCOMPARE(tfx::core::uniquePathInDirectory(temp.path(), "report.txt"),
             QDir(temp.path()).filePath("report 3.txt"));
    QCOMPARE(tfx::core::uniquePathInDirectory(temp.path(), "folder"),
             QDir(temp.path()).filePath("folder"));
}

void PathHandlingTest::copyRecursivelyCopiesNestedDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString source = root.filePath("source");
    const QString nested = QDir(source).filePath("nested");
    const QString destination = root.filePath("destination");
    QVERIFY(QDir().mkpath(nested));
    QVERIFY(writeTextFile(QDir(source).filePath("one.txt")));
    QVERIFY(writeTextFile(QDir(nested).filePath("two.txt")));

    QVERIFY(tfx::core::copyRecursively(source, destination));
    QVERIFY(QFileInfo::exists(QDir(destination).filePath("one.txt")));
    QVERIFY(QFileInfo::exists(QDir(destination).filePath("nested/two.txt")));
    QVERIFY(QFileInfo::exists(QDir(source).filePath("one.txt")));
}

void PathHandlingTest::copyRecursivelyFailsWhenDestinationExists()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString source = root.filePath("source.txt");
    const QString destination = root.filePath("destination.txt");
    QVERIFY(writeTextFile(source));
    QVERIFY(writeTextFile(destination));

    QVERIFY(!tfx::core::copyRecursively(source, destination));
}

QTEST_GUILESS_MAIN(PathHandlingTest)

#include "PathHandlingTest.moc"
