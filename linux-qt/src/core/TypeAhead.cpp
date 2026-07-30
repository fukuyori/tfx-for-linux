#include "core/TypeAhead.h"

#ifdef TFX_ENABLE_RUST_CORE
#include "tfx-bridge/src/lib.rs.h"

#include <QByteArray>

#include <utility>
#endif

namespace tfx::core {

namespace {

TypeAheadStep typeAheadStepCpp(const QStringList &names, int currentRow,
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

#ifdef TFX_ENABLE_RUST_CORE
rust::String toRustString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return rust::String(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}
#endif

}

TypeAheadStep typeAheadStep(const QStringList &names, int currentRow,
                            const QString &prefix, const QString &typed)
{
#ifdef TFX_ENABLE_RUST_CORE
    rust::Vec<rust::String> rustNames;
    rustNames.reserve(static_cast<std::size_t>(names.size()));
    for (const QString &name : names) {
        rustNames.push_back(toRustString(name));
    }

    tfx::rust_bridge::TypeAheadResult result = tfx::rust_bridge::type_ahead_step(
        std::move(rustNames), currentRow, toRustString(prefix), toRustString(typed));
    if (result.error == tfx::rust_bridge::BridgeErrorCode::Ok) {
        return {
            result.row,
            QString::fromUtf8(result.prefix.data(), static_cast<qsizetype>(result.prefix.size())),
        };
    }
#endif
    return typeAheadStepCpp(names, currentRow, prefix, typed);
}

}
