#pragma once

#include "models/FileColumns.h"

#include <QString>
#include <QVector>

// Sort keys offered by the Sort Options dialog. Each key maps onto a file-list
// column; `Natural` is the Name column ordered with numeric-aware collation
// ("file2" before "file10") rather than plain code-point comparison.
namespace tfx::core {

enum class SortKey {
    Name,
    Size,
    DateModified,
    Type,
    Natural,
    DateCreated,
    FileMode,
    GitStatus,
};

struct SortOption
{
    SortKey key = SortKey::Name;
    int column = ColumnName;
    bool natural = false;
};

// Options in the order the dialog lists them.
QVector<SortOption> sortOptions();

// Localized label shown for a key.
QString sortKeyLabel(SortKey key);

// The option backing a key, and the key backing a (column, natural) pair.
SortOption sortOptionFor(SortKey key);
SortKey sortKeyFor(int column, bool natural);

// Row of `sortOptions()` holding the key, or 0 when the key is unknown.
int sortOptionIndex(SortKey key);

// Numeric-aware, case-insensitive name comparison. Returns <0, 0 or >0.
// Ties on the collated form fall back to a case-sensitive comparison so the
// order stays deterministic for names differing only in case.
int naturalCompare(const QString &left, const QString &right);

}
