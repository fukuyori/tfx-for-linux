#include "models/ColumnLayout.h"

#include <QtTest/QtTest>

class ColumnLayoutTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesInvalidColumnOrders();
    void acceptsValidColumnOrders();
    void normalizesWidthsAndSortState();
};

void ColumnLayoutTest::normalizesInvalidColumnOrders()
{
    const QStringList defaults = tfx::models::defaultColumnOrder();

    QCOMPARE(tfx::models::normalizedColumnOrder({"0", "1"}), defaults);
    QCOMPARE(tfx::models::normalizedColumnOrder({"0", "1", "1", "3", "4", "5", "6"}), defaults);
    QCOMPARE(tfx::models::normalizedColumnOrder({"0", "1", "2", "3", "4", "5", "99"}), defaults);
    QCOMPARE(tfx::models::normalizedColumnOrder({"0", "1", "two", "3", "4", "5", "6"}), defaults);
}

void ColumnLayoutTest::acceptsValidColumnOrders()
{
    const QStringList order = {"2", "0", "1", "3", "4", "5", "6"};
    QCOMPARE(tfx::models::normalizedColumnOrder(order), order);
}

void ColumnLayoutTest::normalizesWidthsAndSortState()
{
    QCOMPARE(tfx::models::normalizedColumnWidth(12, 120), 120);
    QCOMPARE(tfx::models::normalizedColumnWidth(24, 120), 24);
    QCOMPARE(tfx::models::normalizedColumnWidth(72, 120), 72);

    QCOMPARE(tfx::models::normalizedSortColumn(-1), ColumnName);
    QCOMPARE(tfx::models::normalizedSortColumn(kColumnCount), ColumnName);
    QCOMPARE(tfx::models::normalizedSortColumn(ColumnModified), ColumnModified);

    QCOMPARE(tfx::models::normalizedSortOrder(static_cast<int>(Qt::DescendingOrder)), Qt::DescendingOrder);
    QCOMPARE(tfx::models::normalizedSortOrder(999), Qt::AscendingOrder);
}

QTEST_GUILESS_MAIN(ColumnLayoutTest)

#include "ColumnLayoutTest.moc"
