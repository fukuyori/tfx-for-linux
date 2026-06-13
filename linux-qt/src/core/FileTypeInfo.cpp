#include "core/FileTypeInfo.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStringList>

namespace tfx::core {
namespace {

QString permissionTriplet(QFile::Permissions permissions, QFile::Permission read,
                          QFile::Permission write, QFile::Permission execute)
{
    QString text;
    text.append(permissions.testFlag(read) ? 'r' : '-');
    text.append(permissions.testFlag(write) ? 'w' : '-');
    text.append(permissions.testFlag(execute) ? 'x' : '-');
    return text;
}

}

QString modeString(const QFileInfo &info)
{
    QString text;
    if (info.isSymLink()) {
        text.append('l');
    } else if (info.isDir()) {
        text.append('d');
    } else {
        text.append('-');
    }

    const QFile::Permissions permissions = info.permissions();
    text.append(permissionTriplet(permissions, QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner));
    text.append(permissionTriplet(permissions, QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup));
    text.append(permissionTriplet(permissions, QFile::ReadOther, QFile::WriteOther, QFile::ExeOther));
    return text;
}

QString sizeString(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 Byte").arg(bytes);
    }
    double value = bytes / 1024.0;
    static const QStringList units = {"KiB", "MiB", "GiB", "TiB", "PiB"};
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QString("%1 %2").arg(value, 0, 'f', 2).arg(units.at(unit));
}

QString englishTypeName(const QFileInfo &info)
{
    if (info.isDir()) {
        return QStringLiteral("Folder");
    }

    static const QMimeDatabase mimeDb;
    const QMimeType mime = mimeDb.mimeTypeForFile(info);
    const QString name = mime.name();

    static const QHash<QString, QString> kNames = {
        {"text/plain", "Plain text"},
        {"text/markdown", "Markdown"},
        {"text/html", "HTML document"},
        {"text/csv", "CSV document"},
        {"text/css", "CSS"},
        {"application/json", "JSON"},
        {"application/xml", "XML"},
        {"text/xml", "XML"},
        {"application/javascript", "JavaScript"},
        {"application/pdf", "PDF document"},
        {"application/zip", "ZIP archive"},
        {"application/gzip", "Gzip archive"},
        {"application/x-tar", "Tar archive"},
        {"application/x-shellscript", "Shell script"},
        {"application/octet-stream", "Binary file"},
    };
    const auto it = kNames.constFind(name);
    if (it != kNames.constEnd()) {
        return it.value();
    }

    if (name.startsWith("image/")) {
        return name.mid(6).toUpper() + " image";
    }
    if (name.startsWith("audio/")) {
        return name.mid(6).toUpper() + " audio";
    }
    if (name.startsWith("video/")) {
        return name.mid(6).toUpper() + " video";
    }

    const QString suffix = info.suffix();
    if (!suffix.isEmpty()) {
        return suffix.toUpper() + " file";
    }
    if (name.startsWith("text/")) {
        return QStringLiteral("Plain text");
    }
    return mime.isValid() ? name : QStringLiteral("File");
}

}
