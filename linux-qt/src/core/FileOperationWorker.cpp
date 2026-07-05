#include "core/FileOperationWorker.h"
#include "core/FileOperations.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <cstdio>

namespace {
constexpr qint64 CopyChunkSize = 1024 * 1024;
constexpr QDir::Filters CopyEntryFilters =
    QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System;

bool renamePath(const QString &from, const QString &to)
{
    // rename(2) instead of QFile::rename: replacing an existing destination
    // atomically is the point here, and QFile::rename refuses to overwrite.
    return ::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0;
}

void addUnique(QStringList *items, const QString &item)
{
    if (!item.isEmpty() && !items->contains(item)) {
        items->append(item);
    }
}
}

FileOperationWorker::FileOperationWorker(const QVector<FileOperationRequest> &requests, QObject *parent)
    : QObject(parent)
    , m_requests(requests)
{
}

void FileOperationWorker::run()
{
    m_completed = 0;
    m_total = 0;
    for (const FileOperationRequest &request : m_requests) {
        m_total += countEntries(request.source);
    }
    if (m_total <= 0) {
        m_total = m_requests.size();
    }
    emit prepared(m_total);

    for (const FileOperationRequest &request : m_requests) {
        if (isCanceled()) {
            cleanupCreatedRoots();
            emit canceled(changedDirectories());
            return;
        }

        const QFileInfo sourceInfo(request.source);
        const QFileInfo destinationInfo(request.destination);
        addUnique(&m_changedDirectories, sourceInfo.absolutePath());
        addUnique(&m_changedDirectories, destinationInfo.absolutePath());

        const int requestEntries = qMax(1, countEntries(request.source));

        const QFileInfo occupant(request.destination);
        if (request.overwrite && (occupant.isSymLink() || occupant.exists())) {
            QString errorText;
            if (!replaceDestination(request, requestEntries, &errorText)) {
                if (isCanceled()) {
                    cleanupCreatedRoots();
                    emit canceled(changedDirectories());
                    return;
                }
                const QString message = errorText.isEmpty()
                    ? tr("Could not replace item: %1").arg(occupant.fileName())
                    : errorText;
                emit failed(message, changedDirectories());
                return;
            }
            continue;
        }

        if (request.move && QFile::rename(request.source, request.destination)) {
            m_completed += requestEntries;
            emit progress(m_completed, m_total, request.destination);
            continue;
        }

        m_createdRoots.append(request.destination);
        QString errorText;
        if (!copyPath(request.source, request.destination, &errorText)) {
            if (isCanceled()) {
                cleanupCreatedRoots();
                emit canceled(changedDirectories());
                return;
            }
            removePath(request.destination);
            m_createdRoots.removeAll(request.destination);
            const QString message = errorText.isEmpty()
                ? tr("Could not transfer item: %1").arg(sourceInfo.fileName())
                : errorText;
            emit failed(message, changedDirectories());
            return;
        }

        if (isCanceled()) {
            cleanupCreatedRoots();
            emit canceled(changedDirectories());
            return;
        }

        if (request.move && !removePath(request.source)) {
            const QString message = tr("Copied but could not remove source: %1").arg(sourceInfo.fileName());
            emit failed(message, changedDirectories());
            return;
        }
        m_createdRoots.removeAll(request.destination);
    }

    if (isCanceled()) {
        cleanupCreatedRoots();
        emit canceled(changedDirectories());
        return;
    }

    emit finished(changedDirectories());
}

void FileOperationWorker::cancel()
{
    m_cancelRequested.store(true);
}

int FileOperationWorker::countEntries(const QString &path) const
{
    const QFileInfo info(path);
    if (info.isSymLink()) {
        return 1;
    }
    if (!info.exists()) {
        return 0;
    }
    if (!info.isDir()) {
        return 1;
    }

    int count = 1;
    QDirIterator iterator(path, CopyEntryFilters, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

bool FileOperationWorker::replaceDestination(const FileOperationRequest &request, int requestEntries, QString *errorText)
{
    const QFileInfo sourceInfo(request.source);
    const QFileInfo destinationInfo(request.destination);
    const bool sourceIsRealDir = !sourceInfo.isSymLink() && sourceInfo.isDir();
    const bool destinationIsRealDir = !destinationInfo.isSymLink() && destinationInfo.isDir();

    // Same-filesystem move over a non-directory destination: rename(2)
    // replaces the destination atomically in one step.
    if (request.move && !sourceIsRealDir && !destinationIsRealDir
        && renamePath(request.source, request.destination)) {
        m_completed += requestEntries;
        emit progress(m_completed, m_total, request.destination);
        return true;
    }

    const QString parent = destinationInfo.absolutePath();
    const QString name = destinationInfo.fileName();
    const QString temp = tfx::core::uniquePathInDirectory(
        parent, QStringLiteral(".%1.tfx-replace").arg(name));
    m_createdRoots.append(temp);
    if (!copyPath(request.source, temp, errorText)) {
        removePath(temp);
        m_createdRoots.removeAll(temp);
        return false;
    }

    const QFileInfo tempInfo(temp);
    const bool tempIsRealDir = !tempInfo.isSymLink() && tempInfo.isDir();
    bool swapped = false;
    if (!tempIsRealDir && !destinationIsRealDir) {
        swapped = renamePath(temp, request.destination);
    } else {
        // rename(2) cannot replace a non-empty directory, so move the old
        // destination aside first and roll it back if the swap fails.
        const QString aside = tfx::core::uniquePathInDirectory(
            parent, QStringLiteral(".%1.tfx-old").arg(name));
        if (renamePath(request.destination, aside)) {
            if (renamePath(temp, request.destination)) {
                swapped = true;
                removePath(aside);
            } else {
                renamePath(aside, request.destination);
            }
        }
    }
    if (!swapped) {
        removePath(temp);
        m_createdRoots.removeAll(temp);
        if (errorText) {
            *errorText = tr("Could not replace item: %1").arg(name);
        }
        return false;
    }
    m_createdRoots.removeAll(temp);

    if (request.move && !removePath(request.source)) {
        if (errorText) {
            *errorText = tr("Copied but could not remove source: %1").arg(sourceInfo.fileName());
        }
        return false;
    }
    return true;
}

bool FileOperationWorker::copyPath(const QString &source, const QString &destination, QString *errorText)
{
    const QFileInfo sourceInfo(source);
    if (sourceInfo.isSymLink()) {
        if (isCanceled()) {
            return false;
        }
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
            if (errorText) {
                *errorText = tr("Could not create folder: %1").arg(QFileInfo(destination).absolutePath());
            }
            return false;
        }
        if (!tfx::core::copySymbolicLink(source, destination)) {
            if (errorText) {
                *errorText = tr("Could not copy link: %1").arg(source);
            }
            return false;
        }
        emitStep(destination);
        return true;
    }
    if (sourceInfo.isDir()) {
        return copyDirectory(source, destination, errorText);
    }
    if (!sourceInfo.exists()) {
        if (errorText) {
            *errorText = tr("Could not read item: %1").arg(source);
        }
        return false;
    }
    if (!sourceInfo.isFile()) {
        // Sockets, FIFOs, and device nodes cannot be copied as content;
        // count them as handled so progress stays consistent.
        emitStep(source);
        return true;
    }
    return copyFile(source, destination, errorText);
}

bool FileOperationWorker::copyFile(const QString &source, const QString &destination, QString *errorText)
{
    if (isCanceled()) {
        return false;
    }

    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (errorText) {
            *errorText = tr("Could not read item: %1").arg(source);
        }
        return false;
    }

    const QFileInfo destinationInfo(destination);
    if (!QDir().mkpath(destinationInfo.absolutePath())) {
        if (errorText) {
            *errorText = tr("Could not create folder: %1").arg(destinationInfo.absolutePath());
        }
        return false;
    }

    QFile output(destination);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorText) {
            *errorText = tr("Could not write item: %1").arg(destination);
        }
        return false;
    }

    while (!input.atEnd()) {
        if (isCanceled()) {
            output.close();
            QFile::remove(destination);
            return false;
        }
        const QByteArray chunk = input.read(CopyChunkSize);
        if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
            if (errorText) {
                *errorText = tr("Could not read item: %1").arg(source);
            }
            return false;
        }
        if (output.write(chunk) != chunk.size()) {
            if (errorText) {
                *errorText = tr("Could not write item: %1").arg(destination);
            }
            return false;
        }
    }

    if (!output.flush()) {
        if (errorText) {
            *errorText = tr("Could not write item: %1").arg(destination);
        }
        return false;
    }
    output.close();
    if (output.error() != QFileDevice::NoError) {
        if (errorText) {
            *errorText = tr("Could not write item: %1").arg(destination);
        }
        return false;
    }

    QFile::setPermissions(destination, input.permissions());
    tfx::core::copyTimestamps(source, destination);
    emitStep(destination);
    return true;
}

bool FileOperationWorker::copyDirectory(const QString &source, const QString &destination, QString *errorText)
{
    if (isCanceled()) {
        return false;
    }
    if (!QDir().mkpath(destination)) {
        if (errorText) {
            *errorText = tr("Could not create folder: %1").arg(destination);
        }
        return false;
    }
    emitStep(destination);

    QDirIterator iterator(source, CopyEntryFilters);
    while (iterator.hasNext()) {
        if (isCanceled()) {
            return false;
        }
        iterator.next();
        const QString childSource = iterator.filePath();
        const QString childDestination = QDir(destination).filePath(iterator.fileName());
        if (!copyPath(childSource, childDestination, errorText)) {
            return false;
        }
    }
    // After the children are written, so their writes do not bump the mtime.
    tfx::core::copyTimestamps(source, destination);
    return true;
}

bool FileOperationWorker::removePath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isSymLink()) {
        // Remove the link itself; QDir::removeRecursively on a link to a
        // directory would delete the link target's contents.
        return QFile::remove(path);
    }
    if (!info.exists()) {
        return true;
    }
    return info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
}

void FileOperationWorker::cleanupCreatedRoots()
{
    for (auto it = m_createdRoots.crbegin(); it != m_createdRoots.crend(); ++it) {
        removePath(*it);
    }
}

void FileOperationWorker::emitStep(const QString &path)
{
    ++m_completed;
    emit progress(m_completed, m_total, path);
}

QStringList FileOperationWorker::changedDirectories() const
{
    return m_changedDirectories;
}

bool FileOperationWorker::isCanceled() const
{
    return m_cancelRequested.load();
}
