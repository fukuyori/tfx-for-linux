#include "core/DelimitedText.h"
#include "core/PreviewText.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using tfx::core::DelimitedTable;
using tfx::core::loadTextCapped;
using tfx::core::parseDelimited;

namespace {
bool writeBytes(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}
}

class PreviewTextTest : public QObject
{
    Q_OBJECT

private slots:
    void loadTextCappedReadsSmallFilesWhole();
    void loadTextCappedTruncatesAtCap();
    void loadTextCappedKeepsUtf8Boundaries();
    void parseDelimitedHandlesQuoting();
    void parseDelimitedCapsRowsAndColumns();
    void parseDelimitedSkipsEmptyLines();
};

void PreviewTextTest::loadTextCappedReadsSmallFilesWhole()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath("small.txt");
    QVERIFY(writeBytes(path, "line1\r\nline2\n"));

    const auto loaded = loadTextCapped(path, 1024);
    QVERIFY(loaded.ok);
    QVERIFY(!loaded.truncated);
    QCOMPARE(loaded.text, QString("line1\nline2\n"));

    QVERIFY(!loadTextCapped(QDir(temp.path()).filePath("missing.txt"), 1024).ok);
}

void PreviewTextTest::loadTextCappedTruncatesAtCap()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath("big.txt");
    QVERIFY(writeBytes(path, QByteArray(4096, 'a')));

    const auto loaded = loadTextCapped(path, 1000);
    QVERIFY(loaded.ok);
    QVERIFY(loaded.truncated);
    QCOMPARE(loaded.text.size(), 1000);
}

void PreviewTextTest::loadTextCappedKeepsUtf8Boundaries()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath("utf8.txt");
    // "あ" is 3 bytes in UTF-8; a 4-byte cap cuts the second glyph in half.
    QVERIFY(writeBytes(path, QString("ああ").toUtf8()));

    const auto loaded = loadTextCapped(path, 4);
    QVERIFY(loaded.ok);
    QVERIFY(loaded.truncated);
    QCOMPARE(loaded.text, QString("あ"));
}

void PreviewTextTest::parseDelimitedHandlesQuoting()
{
    const DelimitedTable table = parseDelimited(
        "name,note\n"
        "\"a,b\",\"line1\nline2\"\n"
        "\"say \"\"hi\"\"\",plain\n",
        ',', 100, 100);

    QCOMPARE(table.rows.size(), 3);
    QCOMPARE(table.rows.at(1), (QStringList{"a,b", "line1\nline2"}));
    QCOMPARE(table.rows.at(2), (QStringList{"say \"hi\"", "plain"}));
    QVERIFY(!table.rowsTruncated);
    QVERIFY(!table.columnsTruncated);
}

void PreviewTextTest::parseDelimitedCapsRowsAndColumns()
{
    QString manyRows;
    for (int index = 0; index < 20; ++index) {
        manyRows += QString("row%1,x\n").arg(index);
    }
    const DelimitedTable rows = parseDelimited(manyRows, ',', 10, 100);
    QCOMPARE(rows.rows.size(), 10);
    QVERIFY(rows.rowsTruncated);

    const DelimitedTable columns = parseDelimited("a,b,c,d,e\n", ',', 10, 3);
    QCOMPARE(columns.rows.first().size(), 3);
    QVERIFY(columns.columnsTruncated);
}

void PreviewTextTest::parseDelimitedSkipsEmptyLines()
{
    const DelimitedTable table = parseDelimited("a,b\n\n\nc,d\n", ',', 100, 100);
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows.at(1), (QStringList{"c", "d"}));

    // A line of delimiters still counts as a (multi-field) row.
    const DelimitedTable commas = parseDelimited(",\n", ',', 100, 100);
    QCOMPARE(commas.rows.size(), 1);
    QCOMPARE(commas.rows.first().size(), 2);
}

QTEST_GUILESS_MAIN(PreviewTextTest)

#include "PreviewTextTest.moc"
