#include "models/ColumnLayout.h"

#include <QSet>

namespace tfx::models {

QStringList defaultColumnOrder()
{
    QStringList order;
    order.reserve(kColumnCount);
    for (int column = 0; column < kColumnCount; ++column) {
        order.append(QString::number(column));
    }
    return order;
}

QStringList normalizedColumnOrder(const QStringList &savedOrder)
{
    if (savedOrder.size() != kColumnCount) {
        return defaultColumnOrder();
    }

    QSet<int> seen;
    for (const QString &item : savedOrder) {
        bool ok = false;
        const int column = item.toInt(&ok);
        if (!ok || column < 0 || column >= kColumnCount || seen.contains(column)) {
            return defaultColumnOrder();
        }
        seen.insert(column);
    }
    return savedOrder;
}

int normalizedColumnWidth(int width, int fallback)
{
    return width >= 24 ? width : fallback;
}

int normalizedSortColumn(int column)
{
    return column >= 0 && column < kColumnCount ? column : ColumnName;
}

Qt::SortOrder normalizedSortOrder(int order)
{
    return order == static_cast<int>(Qt::DescendingOrder)
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;
}

}
