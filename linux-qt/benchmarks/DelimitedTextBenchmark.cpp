#include "core/DelimitedText.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

namespace {
constexpr int DefaultRows = 1000;
constexpr int DefaultColumns = 100;
constexpr int DefaultIterations = 20;

int positiveArgument(const QStringList &arguments, int index, int fallback)
{
    if (index >= arguments.size()) {
        return fallback;
    }

    bool ok = false;
    const int value = arguments.at(index).toInt(&ok);
    return ok && value > 0 ? value : -1;
}

QString makeCsv(int rows, int columns)
{
    QString content;
    content.reserve(static_cast<qsizetype>(rows) * columns * 12);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            if (column > 0) {
                content += QLatin1Char(',');
            }
            if (column % 10 == 0) {
                content += QStringLiteral("\"row%1,cell%2\"").arg(row).arg(column);
            } else {
                content += QStringLiteral("row%1-cell%2").arg(row).arg(column);
            }
        }
        content += QLatin1Char('\n');
    }
    return content;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int rows = positiveArgument(arguments, 1, DefaultRows);
    const int columns = positiveArgument(arguments, 2, DefaultColumns);
    const int iterations = positiveArgument(arguments, 3, DefaultIterations);
    if (rows <= 0 || columns <= 0 || iterations <= 0) {
        QTextStream(stderr) << "Usage: " << arguments.first()
                            << " [positive-rows] [positive-columns] [positive-iterations]\n";
        return 2;
    }

    const QString content = makeCsv(rows, columns);
    tfx::core::parseDelimited(content, QLatin1Char(','), rows, columns);

    qint64 checksum = 0;
    QElapsedTimer timer;
    timer.start();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const tfx::core::DelimitedTable table =
            tfx::core::parseDelimited(content, QLatin1Char(','), rows, columns);
        checksum += table.rows.size();
        if (!table.rows.isEmpty()) {
            checksum += table.rows.first().size();
        }
    }
    const qint64 elapsedNanoseconds = timer.nsecsElapsed();

#ifdef TFX_ENABLE_RUST_CORE
    const char *implementation = "rust";
#else
    const char *implementation = "cpp";
#endif

    QTextStream(stdout) << "implementation=" << implementation
                        << " rows=" << rows
                        << " columns=" << columns
                        << " iterations=" << iterations
                        << " input_utf16_units=" << content.size()
                        << " total_ns=" << elapsedNanoseconds
                        << " ns_per_parse=" << (elapsedNanoseconds / iterations)
                        << " checksum=" << checksum << '\n';
    return checksum == static_cast<qint64>(rows + columns) * iterations ? 0 : 1;
}
