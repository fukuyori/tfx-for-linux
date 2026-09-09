#include "Trash.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/fs.h>

namespace tfx::core {
namespace {
bool privateDirectory(const QString &path)
{
    struct stat st;
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0
        && S_ISDIR(st.st_mode) && st.st_uid == ::getuid()
        && !(st.st_mode & (S_IWGRP | S_IWOTH));
}

bool validLocation(const TrashLocation &location)
{
    const QDir dir(location.path);
    return privateDirectory(location.path) && privateDirectory(dir.filePath("files"))
        && privateDirectory(dir.filePath("info"));
}

TrashEntry readEntry(const TrashLocation &location, const QString &id)
{
    TrashEntry entry{location, id, {}, {}, {}};
    entry.isDirectory = QFileInfo(QDir(location.path).filePath("files/" + id)).isDir();
    const QString infoPath = QDir(location.path).filePath("info/" + id + ".trashinfo");
    const QFileInfo info(infoPath);
    QFile file(infoPath);
    if (info.isSymLink() || !info.isFile() || info.ownerId() != ::getuid()
        || info.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
        entry.error = QStringLiteral("Missing or unreadable trash information: %1").arg(infoPath);
        return entry;
    }
    if (file.readLine().trimmed() != "[Trash Info]") {
        entry.error = QStringLiteral("Invalid trash information: %1").arg(infoPath);
        return entry;
    }
    QByteArray path;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        if (line.endsWith('\n')) line.chop(1);
        if (line.endsWith('\r')) line.chop(1);
        if (line.startsWith('[')) break;
        if (line.startsWith("Path=")) path = QByteArray::fromPercentEncoding(line.mid(5));
        if (line.startsWith("DeletionDate="))
            entry.deletionDate = QDateTime::fromString(QString::fromLatin1(line.mid(13)), Qt::ISODate);
    }
    const QString decoded = QFile::decodeName(path);
    if (path.isEmpty() || path.contains('\0') || decoded.split('/').contains("..")) {
        entry.error = QStringLiteral("Invalid original path: %1").arg(infoPath);
        return entry;
    }
    entry.originalPath = QDir::cleanPath(QDir::isAbsolutePath(decoded)
        ? decoded : QDir(location.basePath).absoluteFilePath(decoded));
    if (entry.originalPath == "/" || entry.originalPath == QDir::cleanPath(location.path)
        || entry.originalPath.startsWith(QDir::cleanPath(location.path) + '/')) {
        entry.error = QStringLiteral("Invalid restore destination: %1").arg(entry.originalPath);
    }
    return entry;
}
}

QVector<TrashLocation> trashLocations()
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QVector<TrashLocation> result{{QDir(dataHome).filePath("Trash"), dataHome}};
    QSet<QString> seen{result.first().path};
    const QString uid = QString::number(::getuid());
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        const QDir root(storage.rootPath());
        const QString shared = root.filePath(".Trash");
        struct stat st;
        QStringList candidates{root.filePath(".Trash-" + uid)};
        if (::lstat(QFile::encodeName(shared).constData(), &st) == 0
            && S_ISDIR(st.st_mode) && (st.st_mode & S_ISVTX)) {
            candidates.append(QDir(shared).filePath(uid));
        }
        for (const QString &path : candidates) {
            if (!seen.contains(path)) {
                seen.insert(path);
                result.append({path, root.absolutePath()});
            }
        }
    }
    return result;
}

TrashListing listTrash(const QVector<TrashLocation> &locations)
{
    TrashListing result;
    for (const auto &location : locations) {
        if (!QFileInfo::exists(location.path) && !QFileInfo(location.path).isSymLink()) continue;
        if (!validLocation(location)) {
            result.errors.append(QStringLiteral("Cannot read trash directory: %1").arg(location.path));
            continue;
        }
        QDir files(QDir(location.path).filePath("files"));
        if (!files.isReadable()) {
            result.errors.append(QStringLiteral("Cannot read trash directory: %1").arg(files.path()));
            continue;
        }
        for (const QString &id : files.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
            result.entries.append(readEntry(location, id));
        }
    }
    return result;
}

bool deleteTrashEntry(const TrashEntry &entry, QString *error)
{
    error->clear();
    if (!validLocation(entry.location) || entry.id.isEmpty() || entry.id.contains('/')
        || entry.id.contains(QChar::Null) || entry.id == "." || entry.id == "..") {
        *error = QStringLiteral("Invalid trash entry.");
        return false;
    }
    const QString source = QDir(entry.location.path).filePath("files/" + entry.id);
    const QFileInfo info(source);
    // Unlink symlinks themselves, including dangling links; never delete their targets.
    const bool removed = info.isDir() && !info.isSymLink()
        ? QDir(source).removeRecursively() : QFile::remove(source);
    if (!removed) {
        *error = QStringLiteral("Could not completely delete: %1").arg(source);
        return false;
    }
    const QString metadata = QDir(entry.location.path).filePath("info/" + entry.id + ".trashinfo");
    if ((QFileInfo::exists(metadata) || QFileInfo(metadata).isSymLink()) && !QFile::remove(metadata)) {
        *error = QStringLiteral("Deleted, but could not remove trash information: %1").arg(metadata);
    }
    return true;
}

bool restoreTrashEntry(const TrashEntry &entry, QString *error)
{
    error->clear();
    if (!validLocation(entry.location) || entry.id.isEmpty() || entry.id.contains('/') || entry.id.contains(QChar::Null)
        || entry.id == "." || entry.id == "..") {
        *error = QStringLiteral("Invalid trash entry.");
        return false;
    }
    const auto current = readEntry(entry.location, entry.id);
    if (!current.error.isEmpty() || current.originalPath != entry.originalPath) {
        *error = current.error.isEmpty() ? QStringLiteral("Trash information changed; refresh the list.") : current.error;
        return false;
    }
    const QString source = QDir(entry.location.path).filePath("files/" + entry.id);
    // Linux atomic no-replace rename handles directories and dangling symlinks
    // without copying data or risking an overwrite between a check and a move.
    if (::syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(source).constData(),
                  AT_FDCWD, QFile::encodeName(entry.originalPath).constData(), RENAME_NOREPLACE) != 0) {
        *error = QStringLiteral("%1: %2").arg(entry.originalPath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    const QString infoPath = QDir(entry.location.path).filePath("info/" + entry.id + ".trashinfo");
    if (!QFile::remove(infoPath)) {
        *error = QStringLiteral("Restored, but could not remove trash information: %1").arg(infoPath);
    }
    return true;
}
}
