#include "core/FileOperationWorker.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool setFileTimes(const QString &path, time_t seconds, bool onLinkItself = false)
{
    struct timespec times[2];
    times[0].tv_sec = seconds;
    times[0].tv_nsec = 0;
    times[1] = times[0];
    return ::utimensat(AT_FDCWD, QFile::encodeName(path).constData(), times,
                       onLinkItself ? AT_SYMLINK_NOFOLLOW : 0) == 0;
}

time_t mtimeOf(const QString &path, bool onLinkItself = false)
{
    struct stat st;
    const QByteArray encoded = QFile::encodeName(path);
    const int result = onLinkItself ? ::lstat(encoded.constData(), &st)
                                    : ::stat(encoded.constData(), &st);
    return result == 0 ? st.st_mtim.tv_sec : -1;
}

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

QStringList hiddenWorkFiles(const QString &directory)
{
    QStringList leftovers;
    const QStringList entries = QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (entry.contains(".tfx-replace") || entry.contains(".tfx-old")) {
            leftovers.append(entry);
        }
    }
    return leftovers;
}

bool writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << text;
    return true;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}
}

class FileOperationWorkerTest : public QObject
{
    Q_OBJECT

private slots:
    void copiesDirectoryTrees();
    void movesFilesWithRename();
    void movesFilesWithCopyFallbackWhenRenameCannotCreateParent();
    void cancelsBeforeStarting();
    void cancelsDuringDirectoryCopyAndCleansDestination();
    void copiesSymlinksAsLinks();
    void movingDirectorySymlinkKeepsTargetContents();
    void copiesHiddenFilesInDirectories();
    void overwriteReplacesExistingFile();
    void overwriteKeepsExistingFileWhenCopyFails();
    void overwriteReplacesExistingDirectory();
    void overwriteMoveReplacesFileAndRemovesSource();
    void writeErrorFailsCopy();
    void copyPreservesTimestamps();
};

void FileOperationWorkerTest::copiesDirectoryTrees()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString nestedRoot = QDir(sourceRoot).filePath("nested");
    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    QVERIFY(QDir().mkpath(nestedRoot));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath("one.txt"), "one"));
    QVERIFY(writeTextFile(QDir(nestedRoot).filePath("two.txt"), "two"));

    FileOperationWorker worker({{sourceRoot, destinationRoot, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);
    QSignalSpy canceledSpy(&worker, &FileOperationWorker::canceled);
    QSignalSpy preparedSpy(&worker, &FileOperationWorker::prepared);
    QSignalSpy progressSpy(&worker, &FileOperationWorker::progress);

    worker.run();

    QCOMPARE(preparedSpy.count(), 1);
    QCOMPARE(preparedSpy.first().at(0).toInt(), 4);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(canceledSpy.count(), 0);
    QVERIFY(progressSpy.count() >= 3);
    QCOMPARE(readTextFile(QDir(destinationRoot).filePath("one.txt")), QString("one"));
    QCOMPARE(readTextFile(QDir(destinationRoot).filePath("nested/two.txt")), QString("two"));
    QVERIFY(QFileInfo::exists(QDir(sourceRoot).filePath("one.txt")));
}

void FileOperationWorkerTest::movesFilesWithRename()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("destination.txt");
    QVERIFY(writeTextFile(source, "move me"));

    FileOperationWorker worker({{source, destination, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(!QFileInfo::exists(source));
    QCOMPARE(readTextFile(destination), QString("move me"));
}

void FileOperationWorkerTest::movesFilesWithCopyFallbackWhenRenameCannotCreateParent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("missing-parent/destination.txt");
    QVERIFY(writeTextFile(source, "fallback move"));

    FileOperationWorker worker({{source, destination, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);
    QSignalSpy canceledSpy(&worker, &FileOperationWorker::canceled);

    worker.run();

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(canceledSpy.count(), 0);
    QVERIFY(!QFileInfo::exists(source));
    QCOMPARE(readTextFile(destination), QString("fallback move"));
}

void FileOperationWorkerTest::cancelsBeforeStarting()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("destination.txt");
    QVERIFY(writeTextFile(source, "stay put"));

    FileOperationWorker worker({{source, destination, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy canceledSpy(&worker, &FileOperationWorker::canceled);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.cancel();
    worker.run();

    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(canceledSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(source));
    QVERIFY(!QFileInfo::exists(destination));
}

void FileOperationWorkerTest::cancelsDuringDirectoryCopyAndCleansDestination()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath("one.txt"), "one"));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath("two.txt"), "two"));

    FileOperationWorker worker({{sourceRoot, destinationRoot, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);
    QSignalSpy canceledSpy(&worker, &FileOperationWorker::canceled);
    QSignalSpy progressSpy(&worker, &FileOperationWorker::progress);
    connect(&worker, &FileOperationWorker::progress, &worker, &FileOperationWorker::cancel);

    worker.run();

    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(canceledSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QVERIFY(QFileInfo::exists(QDir(sourceRoot).filePath("one.txt")));
    QVERIFY(QFileInfo::exists(QDir(sourceRoot).filePath("two.txt")));
    QVERIFY(!QFileInfo::exists(destinationRoot));
}

void FileOperationWorkerTest::copiesSymlinksAsLinks()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString subDir = QDir(sourceRoot).filePath("sub");
    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    QVERIFY(QDir().mkpath(subDir));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath("data.txt"), "payload"));
    QVERIFY(writeTextFile(QDir(subDir).filePath("inner.txt"), "inner"));
    QVERIFY(makeSymlink("data.txt", QDir(sourceRoot).filePath("rel-link")));
    QVERIFY(makeSymlink("sub", QDir(sourceRoot).filePath("dir-link")));
    QVERIFY(makeSymlink("missing-target", QDir(sourceRoot).filePath("broken")));

    FileOperationWorker worker({{sourceRoot, destinationRoot, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);
    QSignalSpy preparedSpy(&worker, &FileOperationWorker::prepared);

    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    // root + data.txt + sub + inner.txt + rel-link + dir-link + broken
    QCOMPARE(preparedSpy.first().at(0).toInt(), 7);

    const QString relLink = QDir(destinationRoot).filePath("rel-link");
    QVERIFY(QFileInfo(relLink).isSymLink());
    QCOMPARE(rawLinkText(relLink), QString("data.txt"));

    const QString dirLink = QDir(destinationRoot).filePath("dir-link");
    QVERIFY(QFileInfo(dirLink).isSymLink());
    QCOMPARE(rawLinkText(dirLink), QString("sub"));
    // The link target's contents were not duplicated through the link.
    QCOMPARE(readTextFile(QDir(destinationRoot).filePath("sub/inner.txt")), QString("inner"));

    const QString broken = QDir(destinationRoot).filePath("broken");
    QVERIFY(QFileInfo(broken).isSymLink());
    QCOMPARE(rawLinkText(broken), QString("missing-target"));
}

void FileOperationWorkerTest::movingDirectorySymlinkKeepsTargetContents()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString targetDir = QDir(temp.path()).filePath("target");
    QVERIFY(QDir().mkpath(targetDir));
    QVERIFY(writeTextFile(QDir(targetDir).filePath("keep.txt"), "keep"));

    const QString link = QDir(temp.path()).filePath("link");
    QVERIFY(makeSymlink(targetDir, link));

    // A destination with a missing parent forces the copy-then-remove-source
    // fallback, which must delete only the link, never the link target.
    const QString destination = QDir(temp.path()).filePath("new-parent/link");

    FileOperationWorker worker({{link, destination, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(QFileInfo(destination).isSymLink());
    QVERIFY(!QFileInfo(link).isSymLink());
    QCOMPARE(readTextFile(QDir(targetDir).filePath("keep.txt")), QString("keep"));
}

void FileOperationWorkerTest::copiesHiddenFilesInDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath(".hidden"), "dot"));

    FileOperationWorker worker({{sourceRoot, destinationRoot, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);

    worker.run();

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(readTextFile(QDir(destinationRoot).filePath(".hidden")), QString("dot"));
}

void FileOperationWorkerTest::overwriteReplacesExistingFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("destination.txt");
    QVERIFY(writeTextFile(source, "new"));
    QVERIFY(writeTextFile(destination, "old"));

    FileOperationWorker worker({{source, destination, false, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(readTextFile(destination), QString("new"));
    QCOMPARE(readTextFile(source), QString("new"));
    QVERIFY(hiddenWorkFiles(temp.path()).isEmpty());
}

void FileOperationWorkerTest::overwriteKeepsExistingFileWhenCopyFails()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("destination.txt");
    QVERIFY(writeTextFile(source, "new"));
    QVERIFY(writeTextFile(destination, "old"));
    QVERIFY(QFile::setPermissions(source, QFileDevice::WriteOwner));
    {
        QFile probe(source);
        if (probe.open(QIODevice::ReadOnly)) {
            QSKIP("Source stays readable (running as root); cannot exercise the failure path.");
        }
    }

    FileOperationWorker worker({{source, destination, false, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(readTextFile(destination), QString("old"));
    QVERIFY(hiddenWorkFiles(temp.path()).isEmpty());
}

void FileOperationWorkerTest::overwriteReplacesExistingDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    QVERIFY(QDir().mkpath(sourceRoot));
    QVERIFY(QDir().mkpath(destinationRoot));
    QVERIFY(writeTextFile(QDir(sourceRoot).filePath("new.txt"), "new"));
    QVERIFY(writeTextFile(QDir(destinationRoot).filePath("old.txt"), "old"));

    FileOperationWorker worker({{sourceRoot, destinationRoot, false, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(readTextFile(QDir(destinationRoot).filePath("new.txt")), QString("new"));
    QVERIFY(!QFileInfo::exists(QDir(destinationRoot).filePath("old.txt")));
    QVERIFY(hiddenWorkFiles(temp.path()).isEmpty());
}

void FileOperationWorkerTest::overwriteMoveReplacesFileAndRemovesSource()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = QDir(temp.path()).filePath("source.txt");
    const QString destination = QDir(temp.path()).filePath("destination.txt");
    QVERIFY(writeTextFile(source, "new"));
    QVERIFY(writeTextFile(destination, "old"));

    FileOperationWorker worker({{source, destination, true, true}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(readTextFile(destination), QString("new"));
    QVERIFY(!QFileInfo::exists(source));
    QVERIFY(hiddenWorkFiles(temp.path()).isEmpty());
}

void FileOperationWorkerTest::writeErrorFailsCopy()
{
    if (::geteuid() == 0) {
        QSKIP("Not safe to exercise /dev/full while running as root.");
    }
    const QFileInfo devFull("/dev/full");
    if (!devFull.exists() || !devFull.isWritable()) {
        QSKIP("/dev/full is unavailable.");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = QDir(temp.path()).filePath("source.txt");
    QVERIFY(writeTextFile(source, "does not fit"));

    FileOperationWorker worker({{source, "/dev/full", false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);

    worker.run();

    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
}

void FileOperationWorkerTest::copyPreservesTimestamps()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceRoot = QDir(temp.path()).filePath("source");
    const QString nestedRoot = QDir(sourceRoot).filePath("nested");
    const QString filePath = QDir(nestedRoot).filePath("one.txt");
    const QString linkPath = QDir(sourceRoot).filePath("link");
    QVERIFY(QDir().mkpath(nestedRoot));
    QVERIFY(writeTextFile(filePath, "one"));
    QVERIFY(makeSymlink("nested/one.txt", linkPath));

    // Back-date everything; directories last so creating entries does not
    // bump them again.
    QVERIFY(setFileTimes(filePath, 1600000000));
    QVERIFY(setFileTimes(linkPath, 1610000000, true));
    QVERIFY(setFileTimes(nestedRoot, 1620000000));
    QVERIFY(setFileTimes(sourceRoot, 1630000000));

    const QString destinationRoot = QDir(temp.path()).filePath("destination");
    FileOperationWorker worker({{sourceRoot, destinationRoot, false}});
    QSignalSpy finishedSpy(&worker, &FileOperationWorker::finished);
    QSignalSpy failedSpy(&worker, &FileOperationWorker::failed);
    worker.run();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(mtimeOf(QDir(destinationRoot).filePath("nested/one.txt")), time_t(1600000000));
    QCOMPARE(mtimeOf(QDir(destinationRoot).filePath("link"), true), time_t(1610000000));
    QCOMPARE(mtimeOf(QDir(destinationRoot).filePath("nested")), time_t(1620000000));
    QCOMPARE(mtimeOf(destinationRoot), time_t(1630000000));
}

QTEST_MAIN(FileOperationWorkerTest)

#include "FileOperationWorkerTest.moc"
