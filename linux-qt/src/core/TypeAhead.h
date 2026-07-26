#pragma once

#include <QString>
#include <QStringList>

namespace tfx::core {

struct TypeAheadStep
{
    int row = -1;     // row to select; -1 keeps the current selection
    QString prefix;   // active prefix after the keystroke
};

// One keystroke of Explorer/Finder-style type-to-select over `names` (in
// display order). Repeating the single-character prefix cycles through rows
// with that initial, starting after `currentRow` and wrapping; otherwise the
// extended prefix jumps to its first match from the top. A keystroke that
// matches nothing keeps both the prefix and the selection.
TypeAheadStep typeAheadStep(const QStringList &names, int currentRow,
                            const QString &prefix, const QString &typed);

}
