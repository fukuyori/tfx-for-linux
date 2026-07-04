#include "core/FileOperations.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <unistd.h>

namespace {
bool makeSymlink(const QString &linkText, const QString &linkPath)
{
    return ::symlink(QFile::encodeName(linkText).constData(),
                     QFile::encodeName(linkPath).constData()) == 0;
}

QString rawLinkText(const QString &path)
{
    QByteArray buffer(4096, '\0');
    const ssize_t length = ::readlink(QFile::encodeName(path).constData(), buffer.data(), buffer.size());
    if (length < 0) {
        return QString();
    }
    return QFile::decodeName(buffer.left(static_cast<int>(length)));
}

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
    void copyRecursivelyPreservesSymlinksAndHiddenFiles();
    void transferWouldNestInsideSourceDetectsDescendants();
    void transferWouldNestInsideSourceResolvesSymlinkedTargets();
    void renameWithinDirectoryHandlesCaseOnlyRenames();
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

void PathHandlingTest::copyRecursivelyPreservesSymlinksAndHiddenFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString source = root.filePath("source");
    const QString destination = root.filePath("destination");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(writeTextFile(QDir(source).filePath("data.txt")));
    QVERIFY(writeTextFile(QDir(source).filePath(".hidden")));
    QVERIFY(makeSymlink("data.txt", QDir(source).filePath("rel-link")));

    QVERIFY(tfx::core::copyRecursively(source, destination));
    QVERIFY(QFileInfo::exists(QDir(destination).filePath(".hidden")));
    const QString link = QDir(destination).filePath("rel-link");
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(rawLinkText(link), QString("data.txt"));
}

void PathHandlingTest::transferWouldNestInsideSourceDetectsDescendants()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString source = root.filePath("source");
    const QString nested = QDir(source).filePath("nested");
    const QString sibling = root.filePath("sibling");
    const QString file = root.filePath("file.txt");
    QVERIFY(QDir().mkpath(nested));
    QVERIFY(QDir().mkpath(sibling));
    QVERIFY(writeTextFile(file));

    QVERIFY(tfx::core::transferWouldNestInsideSource(source, source));
    QVERIFY(tfx::core::transferWouldNestInsideSource(source, nested));
    QVERIFY(!tfx::core::transferWouldNestInsideSource(source, sibling));
    QVERIFY(!tfx::core::transferWouldNestInsideSource(file, source));
}

void PathHandlingTest::transferWouldNestInsideSourceResolvesSymlinkedTargets()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString source = root.filePath("source");
    const QString nested = QDir(source).filePath("nested");
    QVERIFY(QDir().mkpath(nested));

    // A symlink outside the source that points inside it must still be
    // recognized as nesting.
    const QString alias = root.filePath("alias");
    QVERIFY(makeSymlink(nested, alias));
    QVERIFY(tfx::core::transferWouldNestInsideSource(source, alias));

    // A symlink to a directory is copied as a link, so it can never nest.
    const QString sourceLink = root.filePath("source-link");
    QVERIFY(makeSymlink(source, sourceLink));
    QVERIFY(!tfx::core::transferWouldNestInsideSource(sourceLink, nested));
}

void PathHandlingTest::renameWithinDirectoryHandlesCaseOnlyRenames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir root(temp.path());

    // Ordinary rename.
    QVERIFY(writeTextFile(root.filePath("alpha.txt")));
    QVERIFY(tfx::core::renameWithinDirectory(temp.path(), "alpha.txt", "beta.txt"));
    QVERIFY(QFileInfo::exists(root.filePath("beta.txt")));
    QVERIFY(!QFileInfo::exists(root.filePath("alpha.txt")));

    // Case-only rename hops through a temp name and leaves nothing behind.
    QVERIFY(tfx::core::renameWithinDirectory(temp.path(), "beta.txt", "Beta.TXT"));
    const QStringList entries = root.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    QCOMPARE(entries, QStringList{"Beta.TXT"});

    // Same name is a no-op success; renaming a missing file fails.
    QVERIFY(tfx::core::renameWithinDirectory(temp.path(), "Beta.TXT", "Beta.TXT"));
    QVERIFY(!tfx::core::renameWithinDirectory(temp.path(), "missing.txt", "other.txt"));
    QVERIFY(!tfx::core::renameWithinDirectory(temp.path(), "Beta.TXT", "sub/evil.txt"));
}

QTEST_GUILESS_MAIN(PathHandlingTest)

#include "PathHandlingTest.moc"
