#include "platform/Platform.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

class PlatformSecurityTest : public QObject
{
    Q_OBJECT

private slots:
    void zipEntryPathSafety_data();
    void zipEntryPathSafety();
    void inspectZipArchiveDetectsSymlinksAndSizes();
};

void PlatformSecurityTest::zipEntryPathSafety_data()
{
    QTest::addColumn<QString>("entry");
    QTest::addColumn<bool>("safe");

    QTest::newRow("plain file") << QStringLiteral("folder/file.txt") << true;
    QTest::newRow("directory") << QStringLiteral("folder/sub/") << true;
    QTest::newRow("absolute path") << QStringLiteral("/tmp/file.txt") << false;
    QTest::newRow("parent traversal") << QStringLiteral("../file.txt") << false;
    QTest::newRow("nested parent traversal") << QStringLiteral("folder/../file.txt") << false;
    QTest::newRow("current directory segment") << QStringLiteral("folder/./file.txt") << false;
    QTest::newRow("empty segment") << QStringLiteral("folder//file.txt") << false;
    QTest::newRow("backslash traversal") << QStringLiteral("folder\\..\\file.txt") << false;
    QTest::newRow("windows drive") << QStringLiteral("C:/temp/file.txt") << false;
    QTest::newRow("option-like name") << QStringLiteral("-d/tmp/evil") << false;
    QTest::newRow("option-like short") << QStringLiteral("-r") << false;
    QTest::newRow("dash inside name") << QStringLiteral("folder/some-file.txt") << true;
}

void PlatformSecurityTest::zipEntryPathSafety()
{
    QFETCH(QString, entry);
    QFETCH(bool, safe);

    QCOMPARE(tfx::platform::zipEntryPathIsSafe(entry), safe);
}

void PlatformSecurityTest::inspectZipArchiveDetectsSymlinksAndSizes()
{
    const QString zipProgram = QStandardPaths::findExecutable("zip");
    const QString unzipProgram = QStandardPaths::findExecutable("unzip");
    if (zipProgram.isEmpty() || unzipProgram.isEmpty()) {
        QSKIP("zip/unzip not available");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir root(temp.path());
    {
        QFile file(root.filePath("plain.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("hello") == 5);
    }
    QVERIFY(QFile::link("/etc/hostname", root.filePath("evil-link")));

    QProcess zip;
    zip.setWorkingDirectory(temp.path());
    zip.start(zipProgram, {"-q", "--symlinks", "test.zip", "plain.txt", "evil-link"});
    QVERIFY(zip.waitForStarted(5000));
    QVERIFY(zip.waitForFinished(10000));
    QCOMPARE(zip.exitCode(), 0);

    const tfx::platform::ZipInspection inspection =
        tfx::platform::inspectZipArchive(root.filePath("test.zip"));
    QVERIFY(inspection.ok);
    QCOMPARE(inspection.entries.size(), 2);
    QVERIFY(inspection.entries.contains("plain.txt"));
    QCOMPARE(inspection.symlinkEntries, QStringList{"evil-link"});
    QVERIFY(inspection.totalUncompressedBytes >= 5);
}

QTEST_GUILESS_MAIN(PlatformSecurityTest)

#include "PlatformSecurityTest.moc"
