#include "core/FileOperationWorker.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
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
    QSignalSpy progressSpy(&worker, &FileOperationWorker::progress);

    worker.run();

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

QTEST_MAIN(FileOperationWorkerTest)

#include "FileOperationWorkerTest.moc"
