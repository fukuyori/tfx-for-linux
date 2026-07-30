#include "core/DelimitedText.h"

#ifdef TFX_ENABLE_RUST_CORE
#include "tfx-bridge/src/lib.rs.h"

#include <QByteArray>

#include <cstdint>
#include <optional>
#include <utility>
#endif

namespace tfx::core {

namespace {

DelimitedTable parseDelimitedCpp(const QString &content, QChar delimiter,
                                  int maxRows, int maxColumns)
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

#ifdef TFX_ENABLE_RUST_CORE
rust::String toRustString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return rust::String(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::optional<DelimitedTable> fromRustResult(const tfx::rust_bridge::DelimitedResult &result)
{
    DelimitedTable table;
    table.rowsTruncated = result.rows_truncated;
    table.columnsTruncated = result.columns_truncated;

    std::size_t cellIndex = 0;
    table.rows.reserve(static_cast<qsizetype>(result.row_lengths.size()));
    for (const std::uint32_t rowLength : result.row_lengths) {
        if (rowLength > result.cells.size() - cellIndex) {
            return std::nullopt;
        }

        QStringList row;
        row.reserve(static_cast<qsizetype>(rowLength));
        for (std::uint32_t column = 0; column < rowLength; ++column) {
            const rust::String &cell = result.cells[cellIndex++];
            row.append(QString::fromUtf8(cell.data(), static_cast<qsizetype>(cell.size())));
        }
        table.rows.append(std::move(row));
    }
    if (cellIndex != result.cells.size()) {
        return std::nullopt;
    }
    return table;
}
#endif

}

DelimitedTable parseDelimited(const QString &content, QChar delimiter,
                               int maxRows, int maxColumns)
{
#ifdef TFX_ENABLE_RUST_CORE
    const tfx::rust_bridge::DelimitedResult result = tfx::rust_bridge::parse_delimited(
        toRustString(content), delimiter.unicode(), maxRows, maxColumns);
    if (result.error == tfx::rust_bridge::BridgeErrorCode::Ok) {
        if (const std::optional<DelimitedTable> table = fromRustResult(result)) {
            return *table;
        }
    } else if (result.error == tfx::rust_bridge::BridgeErrorCode::InvalidInput) {
        DelimitedTable rejected;
        rejected.rowsTruncated = true;
        rejected.columnsTruncated = true;
        return rejected;
    }
#endif
    return parseDelimitedCpp(content, delimiter, maxRows, maxColumns);
}

}
