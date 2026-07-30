#include "core/TypeAhead.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <QTextStream>

namespace {
constexpr int DefaultNameCount = 10000;
constexpr int DefaultIterations = 200;

int positiveArgument(const QStringList &arguments, int index, int fallback)
{
    if (index >= arguments.size()) {
        return fallback;
    }

    bool ok = false;
    const int value = arguments.at(index).toInt(&ok);
    return ok && value > 0 ? value : -1;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int nameCount = positiveArgument(arguments, 1, DefaultNameCount);
    const int iterations = positiveArgument(arguments, 2, DefaultIterations);
    if (nameCount <= 0 || iterations <= 0) {
        QTextStream(stderr) << "Usage: " << arguments.first()
                            << " [positive-name-count] [positive-iterations]\n";
        return 2;
    }

    QStringList names;
    names.reserve(nameCount);
    for (int row = 0; row < nameCount - 1; ++row) {
        names.append(QStringLiteral("item-%1").arg(row, 8, 10, QLatin1Char('0')));
    }
    names.append(QStringLiteral("z-final-match"));

    for (int iteration = 0; iteration < 3; ++iteration) {
        tfx::core::typeAheadStep(names, -1, QString(), QStringLiteral("z"));
    }

    qint64 checksum = 0;
    QElapsedTimer timer;
    timer.start();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const tfx::core::TypeAheadStep result =
            tfx::core::typeAheadStep(names, -1, QString(), QStringLiteral("z"));
        checksum += result.row;
    }
    const qint64 elapsedNanoseconds = timer.nsecsElapsed();

#ifdef TFX_ENABLE_RUST_CORE
    const char *implementation = "rust";
#else
    const char *implementation = "cpp";
#endif

    QTextStream(stdout) << "implementation=" << implementation
                        << " names=" << nameCount
                        << " iterations=" << iterations
                        << " total_ns=" << elapsedNanoseconds
                        << " ns_per_call=" << (elapsedNanoseconds / iterations)
                        << " checksum=" << checksum << '\n';
    return checksum == static_cast<qint64>(nameCount - 1) * iterations ? 0 : 1;
}
