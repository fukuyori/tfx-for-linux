#include "core/SearchState.h"

#include <QtTest/QtTest>

class SearchStateTest : public QObject
{
    Q_OBJECT

private slots:
    void movesNewTermsToFrontAndCollapsesDuplicates();
    void trimsEmptyEntriesAndHonorsLimit();
    void normalizesExistingHistoryWhenTermIsEmpty();
    void rejectsNonPositiveLimits();
};

void SearchStateTest::movesNewTermsToFrontAndCollapsesDuplicates()
{
    QCOMPARE(tfx::core::updatedSearchHistory({"alpha", "beta", "gamma"}, "Beta"),
             QStringList({"Beta", "alpha", "gamma"}));
}

void SearchStateTest::trimsEmptyEntriesAndHonorsLimit()
{
    QCOMPARE(tfx::core::updatedSearchHistory({" alpha ", "", "beta", "gamma"}, " delta ", 3),
             QStringList({"delta", "alpha", "beta"}));
}

void SearchStateTest::normalizesExistingHistoryWhenTermIsEmpty()
{
    QCOMPARE(tfx::core::updatedSearchHistory({" alpha ", "", "beta", " ALPHA ", "gamma"}, "   ", 3),
             QStringList({"alpha", "beta", "gamma"}));
}

void SearchStateTest::rejectsNonPositiveLimits()
{
    QCOMPARE(tfx::core::updatedSearchHistory({"alpha", "beta"}, "gamma", 0), QStringList());
    QCOMPARE(tfx::core::updatedSearchHistory({"alpha", "beta"}, "gamma", -2), QStringList());
}

QTEST_GUILESS_MAIN(SearchStateTest)

#include "SearchStateTest.moc"
