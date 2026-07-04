#include "core/SearchState.h"

#include <QtGlobal>

namespace tfx::core {

namespace {

bool containsTerm(const QStringList &terms, const QString &term)
{
    for (const QString &existing : terms) {
        if (existing.compare(term, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

}

QStringList updatedSearchHistory(const QStringList &history, const QString &term, int limit)
{
    if (limit <= 0) {
        return {};
    }

    QStringList updated;
    updated.reserve(qMin(limit, history.size() + 1));
    const QString trimmed = term.trimmed();
    if (!trimmed.isEmpty()) {
        updated.append(trimmed);
    }
    for (const QString &item : history) {
        const QString candidate = item.trimmed();
        if (candidate.isEmpty() || containsTerm(updated, candidate)) {
            continue;
        }
        updated.append(candidate);
        if (updated.size() >= limit) {
            break;
        }
    }
    return updated;
}

}
