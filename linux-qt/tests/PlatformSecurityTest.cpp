#include "platform/Platform.h"

#include <QtTest/QtTest>

class PlatformSecurityTest : public QObject
{
    Q_OBJECT

private slots:
    void zipEntryPathSafety_data();
    void zipEntryPathSafety();
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
}

void PlatformSecurityTest::zipEntryPathSafety()
{
    QFETCH(QString, entry);
    QFETCH(bool, safe);

    QCOMPARE(tfx::platform::zipEntryPathIsSafe(entry), safe);
}

QTEST_GUILESS_MAIN(PlatformSecurityTest)

#include "PlatformSecurityTest.moc"
