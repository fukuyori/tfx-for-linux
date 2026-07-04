#pragma once

#include "models/FileColumns.h"

#include <QStringList>
#include <Qt>

namespace tfx::models {

QStringList defaultColumnOrder();
QStringList normalizedColumnOrder(const QStringList &savedOrder);
int normalizedColumnWidth(int width, int fallback);
int normalizedSortColumn(int column);
Qt::SortOrder normalizedSortOrder(int order);

}
