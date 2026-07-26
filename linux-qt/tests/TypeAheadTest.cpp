#include "core/TypeAhead.h"

#include <QtTest/QtTest>

using namespace tfx::core;

class TypeAheadTest : public QObject
{
    Q_OBJECT

private slots:
    void firstKeystrokeJumpsToFirstMatch();
    void extendedPrefixNarrowsTheMatch();
    void mismatchKeepsPrefixAndSelection();
    void repeatedInitialCyclesThroughMatchesAndWraps();
    void matchingIsCaseInsensitive();
    void emptyListDoesNothing();
};

void TypeAheadTest::firstKeystrokeJumpsToFirstMatch()
{
    const QStringList names = {"alpha", "beta", "charlie", "chip"};
    const TypeAheadStep step = typeAheadStep(names, -1, QString(), "c");
    QCOMPARE(step.row, 2);
    QCOMPARE(step.prefix, QString("c"));
}

void TypeAheadTest::extendedPrefixNarrowsTheMatch()
{
    const QStringList names = {"alpha", "beta", "charlie", "chip"};
    TypeAheadStep step = typeAheadStep(names, 2, "c", "h");
    QCOMPARE(step.row, 2);
    QCOMPARE(step.prefix, QString("ch"));
    step = typeAheadStep(names, 2, "ch", "i");
    QCOMPARE(step.row, 3);
    QCOMPARE(step.prefix, QString("chi"));
}

void TypeAheadTest::mismatchKeepsPrefixAndSelection()
{
    const QStringList names = {"alpha", "beta", "charlie"};
    const TypeAheadStep step = typeAheadStep(names, 2, "ch", "z");
    QCOMPARE(step.row, -1);
    QCOMPARE(step.prefix, QString("ch"));
}

void TypeAheadTest::repeatedInitialCyclesThroughMatchesAndWraps()
{
    const QStringList names = {"docs", "cats", "code", "songs", "cli"};
    // Prefix "c" active on row 1: the next "c" moves to the following match.
    TypeAheadStep step = typeAheadStep(names, 1, "c", "c");
    QCOMPARE(step.row, 2);
    QCOMPARE(step.prefix, QString("c"));
    step = typeAheadStep(names, 2, "c", "c");
    QCOMPARE(step.row, 4);
    // Wraps past the end back to the first match.
    step = typeAheadStep(names, 4, "c", "c");
    QCOMPARE(step.row, 1);
}

void TypeAheadTest::matchingIsCaseInsensitive()
{
    const QStringList names = {"README.md", "Makefile", "main.cpp"};
    TypeAheadStep step = typeAheadStep(names, -1, QString(), "r");
    QCOMPARE(step.row, 0);
    step = typeAheadStep(names, -1, QString(), "M");
    QCOMPARE(step.row, 1);
    // Cycling with the same initial covers both cases.
    step = typeAheadStep(names, 1, "M", "m");
    QCOMPARE(step.row, 2);
}

void TypeAheadTest::emptyListDoesNothing()
{
    const TypeAheadStep step = typeAheadStep({}, -1, QString(), "a");
    QCOMPARE(step.row, -1);
    QVERIFY(step.prefix.isEmpty());
}

QTEST_GUILESS_MAIN(TypeAheadTest)
#include "TypeAheadTest.moc"
