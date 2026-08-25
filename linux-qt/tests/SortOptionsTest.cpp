#include "core/SortOptions.h"
#include "models/FileColumns.h"

#include <QtTest>

using namespace tfx::core;

// The Sort Options dialog is a thin shell over these mappings: every key it
// lists has to resolve to a column the file list can actually sort by, and the
// round trip back from (column, natural) is what preselects the current row.
class SortOptionsTest : public QObject
{
    Q_OBJECT

private slots:
    void optionsCoverEverySortableColumn();
    void naturalIsTheOnlyNaturalOption();
    void keyRoundTripsThroughColumn();
    void unknownNaturalFlagFallsBackToTheColumn();
    void everyKeyHasALabelAndRow();
    void naturalCompareOrdersEmbeddedNumbers();
    void naturalCompareIsCaseInsensitiveButDeterministic();
};

void SortOptionsTest::optionsCoverEverySortableColumn()
{
    QSet<int> columns;
    for (const SortOption &option : sortOptions()) {
        QVERIFY(option.column >= 0);
        QVERIFY(option.column < kColumnCount);
        columns.insert(option.column);
    }
    // Every file-list column is reachable from the dialog.
    QCOMPARE(columns.size(), static_cast<int>(kColumnCount));
}

void SortOptionsTest::naturalIsTheOnlyNaturalOption()
{
    for (const SortOption &option : sortOptions()) {
        if (option.key == SortKey::Natural) {
            QVERIFY(option.natural);
            QCOMPARE(option.column, static_cast<int>(ColumnName));
        } else {
            QVERIFY(!option.natural);
        }
    }
}

void SortOptionsTest::keyRoundTripsThroughColumn()
{
    for (const SortOption &option : sortOptions()) {
        QCOMPARE(sortKeyFor(option.column, option.natural), option.key);
        const SortOption resolved = sortOptionFor(option.key);
        QCOMPARE(resolved.column, option.column);
        QCOMPARE(resolved.natural, option.natural);
    }
}

void SortOptionsTest::unknownNaturalFlagFallsBackToTheColumn()
{
    // Natural ordering only exists for names, so a stale natural flag on any
    // other column still resolves to that column's own key.
    QCOMPARE(sortKeyFor(ColumnSize, true), SortKey::Size);
    QCOMPARE(sortKeyFor(ColumnModified, true), SortKey::DateModified);
    // An out-of-range column cannot select a row; Name is the safe default.
    QCOMPARE(sortKeyFor(kColumnCount + 3, false), SortKey::Name);
}

void SortOptionsTest::everyKeyHasALabelAndRow()
{
    const QVector<SortOption> options = sortOptions();
    for (int row = 0; row < options.size(); ++row) {
        const SortKey key = options.at(row).key;
        QVERIFY(!sortKeyLabel(key).isEmpty());
        QCOMPARE(sortOptionIndex(key), row);
    }
}

void SortOptionsTest::naturalCompareOrdersEmbeddedNumbers()
{
    // The point of the Natural key: digit runs compare by value, so file2
    // lands before file10 instead of after it.
    QVERIFY(naturalCompare("file2.txt", "file10.txt") < 0);
    QVERIFY(naturalCompare("file10.txt", "file2.txt") > 0);
    QVERIFY(QString::compare("file2.txt", "file10.txt") > 0);

    QVERIFY(naturalCompare("img9", "img10") < 0);
    QVERIFY(naturalCompare("v1.9.0", "v1.10.0") < 0);
    QCOMPARE(naturalCompare("same.txt", "same.txt"), 0);
}

void SortOptionsTest::naturalCompareIsCaseInsensitiveButDeterministic()
{
    // "Beta" sorts between "alpha" and "gamma" rather than ahead of both the
    // way a code-point comparison would place it.
    QVERIFY(naturalCompare("alpha", "Beta") < 0);
    QVERIFY(naturalCompare("Beta", "gamma") < 0);

    // Names differing only in case must still order consistently rather than
    // comparing equal, which would leave their relative order undefined.
    QVERIFY(naturalCompare("readme", "README") != 0);
    QCOMPARE(naturalCompare("readme", "README") > 0, naturalCompare("README", "readme") < 0);
}

QTEST_MAIN(SortOptionsTest)
#include "SortOptionsTest.moc"
