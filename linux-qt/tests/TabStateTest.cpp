#include "core/TabState.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

class TabStateTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesRestoredTabPaths();
    void clampsActiveTabIndex();
};

void TabStateTest::normalizesRestoredTabPaths()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QDir root(temp.path());
    const QString one = root.filePath("one");
    const QString two = root.filePath("two");
    QVERIFY(QDir().mkpath(one));
    QVERIFY(QDir().mkpath(two));

    const QString oneAlias = root.filePath("one-alias");
    QFile::link(one, oneAlias);

    const QStringList normalized = tfx::core::normalizedTabPaths({
        one,
        two,
        oneAlias,
        root.filePath("missing"),
    });

    QCOMPARE(normalized, QStringList({
        QFileInfo(one).canonicalFilePath(),
        QFileInfo(two).canonicalFilePath(),
    }));
}

void TabStateTest::clampsActiveTabIndex()
{
    QCOMPARE(tfx::core::clampedTabIndex(-4, 3), 0);
    QCOMPARE(tfx::core::clampedTabIndex(1, 3), 1);
    QCOMPARE(tfx::core::clampedTabIndex(9, 3), 2);
    QCOMPARE(tfx::core::clampedTabIndex(0, 0), -1);
}

QTEST_GUILESS_MAIN(TabStateTest)

#include "TabStateTest.moc"
