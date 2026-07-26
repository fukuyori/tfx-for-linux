#include "core/TypeAhead.h"

namespace tfx::core {

TypeAheadStep typeAheadStep(const QStringList &names, int currentRow,
                            const QString &prefix, const QString &typed)
{
    TypeAheadStep step;
    step.prefix = prefix;
    const int rows = names.size();
    if (rows <= 0 || typed.isEmpty()) {
        return step;
    }

    if (prefix.size() == 1 && prefix.compare(typed, Qt::CaseInsensitive) == 0) {
        // Same single character again: cycle to the next row with that
        // initial, starting after the current row and wrapping.
        for (int offset = 1; offset <= rows; ++offset) {
            const int row = (currentRow + offset + rows) % rows;
            if (names.at(row).startsWith(prefix, Qt::CaseInsensitive)) {
                step.row = row;
                break;
            }
        }
        return step;
    }

    const QString candidate = prefix + typed;
    for (int row = 0; row < rows; ++row) {
        if (names.at(row).startsWith(candidate, Qt::CaseInsensitive)) {
            step.row = row;
            step.prefix = candidate;
            break;
        }
    }
    // No match: the prefix and the selection stay where they are.
    return step;
}

}
