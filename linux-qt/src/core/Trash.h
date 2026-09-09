#pragma once

#include <QDateTime>
#include <QStringList>
#include <QVector>

namespace tfx::core {
struct TrashLocation {
    QString path;
    QString basePath;
};
struct TrashEntry {
    TrashLocation location;
    QString id;
    QString originalPath;
    QDateTime deletionDate;
    QString error;
    bool isDirectory = false;
};
struct TrashListing {
    QVector<TrashEntry> entries;
    QStringList errors;
};

QVector<TrashLocation> trashLocations();
TrashListing listTrash(const QVector<TrashLocation> &locations);
// Never overwrites a destination. On failure the trashed item is retained.
bool restoreTrashEntry(const TrashEntry &entry, QString *error);
bool deleteTrashEntry(const TrashEntry &entry, QString *error);
}
