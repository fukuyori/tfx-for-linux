#include "FilePane.h"
#include "UiText.h"
#include "core/FileOperations.h"
#include "platform/Platform.h"

#include <QDir>
#include <QEventLoop>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSet>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

using namespace tfx::core;

namespace {

bool runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory, QString *errorText)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);

    // Wait through a local event loop instead of waitForFinished(-1) so the
    // window keeps repainting during a long archive run; user input stays
    // excluded to avoid re-entrancy.
    QEventLoop loop;
    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (errorText) {
            *errorText = process.errorString();
        }
        return false;
    }
    while (process.state() != QProcess::NotRunning) {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return true;
    }
    if (errorText) {
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        *errorText = stderrText.isEmpty() ? process.errorString() : stderrText;
    }
    return false;
}

QString firstUnsafeZipEntry(const QStringList &entries)
{
    for (const QString &entry : entries) {
        if (!tfx::platform::zipEntryPathIsSafe(entry)) {
            return entry;
        }
    }
    return QString();
}

}

void FilePane::openZip(const QString &path)
{
    m_zipPath = path;
    m_zipDir.clear();
    const tfx::platform::ZipInspection inspection = tfx::platform::inspectZipArchive(path);
    m_zipEntries = inspection.entries;
    m_zipSymlinkEntries = inspection.symlinkEntries;
    if (!inspection.ok || m_zipEntries.isEmpty()) {
        m_zipEntries.clear();
        m_zipSymlinkEntries.clear();
        emit statusMessageRequested(UiText::t("Could not read archive.", "アーカイブを読み込めませんでした。"));
        return;
    }
    const QString unsafeEntry = firstUnsafeZipEntry(m_zipEntries);
    if (!unsafeEntry.isEmpty()) {
        m_zipEntries.clear();
        m_zipSymlinkEntries.clear();
        emit statusMessageRequested(
            UiText::t("Archive contains an unsafe path: %1", "アーカイブに安全でないパスが含まれています: %1")
                .arg(unsafeEntry));
        return;
    }
    populateZipView();
    m_viewStack->setCurrentWidget(m_zipView);
}

void FilePane::populateZipView()
{
    m_zipModel->removeRows(0, m_zipModel->rowCount());

    const QIcon folderIcon = m_iconProvider.icon(QFileIconProvider::Folder);
    const QIcon fileIcon = m_iconProvider.icon(QFileIconProvider::File);
    auto *up = new QStandardItem(folderIcon, "..");
    up->setData("..", Qt::UserRole);
    up->setData(true, Qt::UserRole + 1);
    m_zipModel->appendRow({up, new QStandardItem(QString())});

    QSet<QString> dirsSeen;
    const int prefixLen = m_zipDir.size();
    for (const QString &entry : m_zipEntries) {
        if (!entry.startsWith(m_zipDir)) {
            continue;
        }
        const QString rest = entry.mid(prefixLen);
        if (rest.isEmpty()) {
            continue;
        }
        const int slash = rest.indexOf('/');
        if (slash >= 0) {
            const QString dirName = rest.left(slash);
            const QString fullDir = m_zipDir + dirName + "/";
            if (!dirsSeen.contains(fullDir)) {
                dirsSeen.insert(fullDir);
                auto *item = new QStandardItem(folderIcon, dirName);
                item->setData(fullDir, Qt::UserRole);
                item->setData(true, Qt::UserRole + 1);
                m_zipModel->appendRow({item, new QStandardItem(UiText::t("Folder", "フォルダ"))});
            }
        } else {
            auto *item = new QStandardItem(fileIcon, rest);
            item->setData(entry, Qt::UserRole);
            item->setData(false, Qt::UserRole + 1);
            const QString suffix = QFileInfo(rest).suffix();
            m_zipModel->appendRow({item, new QStandardItem(suffix.isEmpty() ? UiText::t("File", "ファイル")
                                                                            : suffix.toUpper() + " file")});
        }
    }

    const QString internal = m_zipDir.isEmpty() ? QString() : "/" + m_zipDir.chopped(1);
    m_pathEdit->setText(displayPath(m_zipPath) + internal);
}

void FilePane::exitZipMode()
{
    const QString zip = m_zipPath;
    if (zip.isEmpty()) {
        return;
    }
    navigateTo(QFileInfo(zip).absolutePath());
    QTimer::singleShot(0, this, [this, zip]() { setCurrentIndexForPath(zip); });
}

void FilePane::compressSelectedItemsToZip()
{
    const QString zipProgram = QStandardPaths::findExecutable("zip");
    if (zipProgram.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("zip command was not found.", "zip コマンドが見つかりません。"));
        return;
    }

    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) {
        return;
    }

    QString archiveBaseName = urls.size() == 1
        ? QFileInfo(urls.first().toLocalFile()).completeBaseName() + ".zip"
        : placeholderText("Archive.zip", "アーカイブ.zip");
    const QString archivePath = uniquePathInDirectory(m_currentPath, archiveBaseName);

    QStringList arguments;
    arguments << "-r" << archivePath << "--"; // "--": a name starting with '-' must not become an option
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            arguments << QFileInfo(path).fileName();
        }
    }

    QString errorText;
    if (!runProcess(zipProgram, arguments, m_currentPath, &errorText)) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Could not create zip archive.\n%1", "zip アーカイブを作成できませんでした。\n%1").arg(errorText));
        return;
    }

    reload();
    setCurrentIndexForPath(archivePath);
    emit statusMessageRequested(UiText::t("Created zip archive.", "zip アーカイブを作成しました。"));
}

void FilePane::extractSelectedZip()
{
    const QString unzipProgram = QStandardPaths::findExecutable("unzip");
    if (unzipProgram.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("unzip command was not found.", "unzip コマンドが見つかりません。"));
        return;
    }

    const QFileInfo archiveInfo = currentFileInfo();
    if (!archiveInfo.exists() || archiveInfo.suffix().compare("zip", Qt::CaseInsensitive) != 0) {
        return;
    }

    const QString destinationPath = uniquePathInDirectory(m_currentPath, archiveInfo.completeBaseName());
    const tfx::platform::ZipInspection inspection = tfx::platform::inspectZipArchive(archiveInfo.absoluteFilePath());
    if (!inspection.ok || inspection.entries.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not read archive.", "アーカイブを読み込めませんでした。"));
        return;
    }
    const QString unsafeEntry = firstUnsafeZipEntry(inspection.entries);
    if (!unsafeEntry.isEmpty()) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Archive contains an unsafe path and was not extracted:\n%1",
                      "アーカイブに安全でないパスが含まれているため展開しませんでした:\n%1")
                .arg(unsafeEntry));
        return;
    }
    // A symlink entry followed by files under its name lets an archive write
    // outside the destination, so refuse archives containing links.
    if (!inspection.symlinkEntries.isEmpty()) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Archive contains a symbolic link and was not extracted:\n%1",
                      "アーカイブにシンボリックリンクが含まれているため展開しませんでした:\n%1")
                .arg(inspection.symlinkEntries.first()));
        return;
    }
    constexpr int MaxEntries = 100000;
    constexpr qint64 MaxTotalBytes = 4LL * 1024 * 1024 * 1024;
    if (inspection.entries.size() > MaxEntries || inspection.totalUncompressedBytes > MaxTotalBytes) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Archive is too large to extract (over %1 entries or %2 GB).",
                      "アーカイブが大きすぎるため展開しませんでした（%1 エントリまたは %2 GB 超）。")
                .arg(MaxEntries)
                .arg(MaxTotalBytes / (1024 * 1024 * 1024)));
        return;
    }

    // Extract into a hidden work directory first so a failed run never leaves
    // a partial tree under the final name.
    const QString workPath = uniquePathInDirectory(
        m_currentPath, "." + QFileInfo(destinationPath).fileName() + ".tfx-extract");
    if (!QDir().mkpath(workPath)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create extraction folder.", "展開先フォルダを作成できませんでした。"));
        return;
    }

    QString errorText;
    const QStringList arguments = { archiveInfo.absoluteFilePath(), "-d", workPath };
    if (!runProcess(unzipProgram, arguments, m_currentPath, &errorText)
        || !QDir().rename(workPath, destinationPath)) {
        QDir(workPath).removeRecursively();
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Could not extract zip archive.\n%1", "zip アーカイブを展開できませんでした。\n%1").arg(errorText));
        return;
    }

    reload();
    setCurrentIndexForPath(destinationPath);
    emit statusMessageRequested(UiText::t("Extracted zip archive.", "zip アーカイブを展開しました。"));
}
