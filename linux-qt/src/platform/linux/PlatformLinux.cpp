#include "platform/Platform.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

namespace tfx::platform {

bool openPath(const QString &path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool openWithChooser(const QString &path)
{
    // xdg-desktop-portal OpenURI with ask = true shows the native chooser.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.OpenURI",
        "OpenFile");
    message << QString()
            << QVariant::fromValue(QDBusUnixFileDescriptor(file.handle()))
            << QVariantMap{{"ask", true}};
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message);
    file.close();
    return reply.type() != QDBusMessage::ErrorMessage;
}

bool revealInFileManager(const QString &path)
{
    const QFileInfo info(path);
    const QString target = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

bool moveToTrash(const QStringList &paths)
{
    bool allOk = true;
    for (const QString &path : paths) {
        if (!QFile::moveToTrash(path)) {
            allOk = false;
        }
    }
    return allOk;
}

QString terminalShellProgram()
{
    return QStringLiteral("/bin/sh");
}

QStringList terminalRunArguments(const QString &command)
{
    return {QStringLiteral("-lc"), command};
}

QStringList listZipEntries(const QString &zipPath)
{
    const QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty()) {
        return {};
    }
    QProcess process;
    process.start(unzip, {"-Z1", zipPath});
    if (!process.waitForStarted(1000) || !process.waitForFinished(8000)
        || process.exitStatus() != QProcess::NormalExit) {
        return {};
    }
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    return output.split('\n', Qt::SkipEmptyParts);
}

bool extractZipEntry(const QString &zipPath, const QString &entry, const QString &destDir)
{
    const QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty()) {
        return false;
    }
    QProcess process;
    process.start(unzip, {"-o", zipPath, entry, "-d", destDir});
    return process.waitForStarted(1000) && process.waitForFinished(15000)
        && process.exitStatus() == QProcess::NormalExit && process.exitCode() <= 1;
}

bool pdfPreviewAvailable()
{
    return !QStandardPaths::findExecutable("pdftoppm").isEmpty();
}

bool renderPdfPreview(const QString &pdfPath, const QString &outputPngPath, int scaleToPx)
{
    const QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (pdftoppm.isEmpty()) {
        return false;
    }
    QString prefix = outputPngPath;
    if (prefix.endsWith(".png", Qt::CaseInsensitive)) {
        prefix.chop(4);
    }
    QProcess process;
    process.start(pdftoppm, {"-f", "1", "-singlefile", "-png",
                             "-scale-to", QString::number(scaleToPx), pdfPath, prefix});
    if (!process.waitForStarted(1000) || !process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }
    return QFileInfo::exists(prefix + ".png");
}

}
