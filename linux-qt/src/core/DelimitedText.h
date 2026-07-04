#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

namespace tfx::core {

struct DelimitedTable
{
    QVector<QStringList> rows;
    bool rowsTruncated = false;
    bool columnsTruncated = false;
};

// Parse comma/tab separated content with RFC 4180 quoting: fields may be
// wrapped in double quotes, `""` inside a quoted field is a literal quote,
// and quoted fields may contain delimiters and newlines. Parsing stops after
// `maxRows` rows; each row keeps at most `maxColumns` fields. Rows that are
// completely empty are skipped.
DelimitedTable parseDelimited(const QString &content, QChar delimiter, int maxRows, int maxColumns);

}
