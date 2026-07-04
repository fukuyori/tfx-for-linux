#pragma once

#include "models/FileColumns.h"

namespace tfx::filepane {

inline int defaultColumnWidth(int column)
{
    switch (column) {
    case ColumnName:
        return 340;
    case ColumnType:
        return 120;
    case ColumnSize:
        return 96;
    case ColumnCreated:
        return 160;
    case ColumnModified:
        return 160;
    case ColumnMode:
        return 116;
    case ColumnGit:
        return 28;
    default:
        return 120;
    }
}

}
