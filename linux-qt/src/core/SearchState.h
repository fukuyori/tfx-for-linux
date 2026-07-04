#pragma once

#include <QString>
#include <QStringList>

namespace tfx::core {

QStringList updatedSearchHistory(const QStringList &history, const QString &term, int limit = 10);

}
