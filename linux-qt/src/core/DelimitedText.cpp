#include "core/DelimitedText.h"

namespace tfx::core {

DelimitedTable parseDelimited(const QString &content, QChar delimiter, int maxRows, int maxColumns)
{
    DelimitedTable table;
    QStringList row;
    QString field;
    bool inQuotes = false;
    bool rowHasContent = false;

    const auto endField = [&]() {
        if (row.size() < maxColumns) {
            row.append(field);
        } else {
            table.columnsTruncated = true;
        }
        rowHasContent = rowHasContent || !field.isEmpty();
        field.clear();
    };
    const auto endRow = [&]() {
        endField();
        const bool emptyRow = !rowHasContent && row.size() <= 1;
        if (!emptyRow) {
            if (table.rows.size() < maxRows) {
                table.rows.append(row);
            } else {
                table.rowsTruncated = true;
            }
        }
        row.clear();
        rowHasContent = false;
    };

    const int size = content.size();
    for (int index = 0; index < size; ++index) {
        const QChar character = content.at(index);
        if (inQuotes) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < size && content.at(index + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++index;
                } else {
                    inQuotes = false;
                }
            } else {
                field.append(character);
            }
            continue;
        }

        if (character == QLatin1Char('"') && field.isEmpty()) {
            inQuotes = true;
            rowHasContent = true; // "" counts as a present (empty) field
        } else if (character == delimiter) {
            endField();
            rowHasContent = true; // a delimiter implies at least two fields
        } else if (character == QLatin1Char('\n') || character == QLatin1Char('\r')) {
            if (character == QLatin1Char('\r') && index + 1 < size
                && content.at(index + 1) == QLatin1Char('\n')) {
                ++index;
            }
            endRow();
            if (table.rows.size() >= maxRows && table.rowsTruncated) {
                return table;
            }
        } else {
            field.append(character);
        }
    }
    if (rowHasContent || !field.isEmpty() || !row.isEmpty()) {
        endRow();
    }
    return table;
}

}
