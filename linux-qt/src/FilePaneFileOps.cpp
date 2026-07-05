#include "FilePane.h"
#include "FilePaneClipboard.h"
#include "UiText.h"
#include "models/FileColumns.h"
#include "platform/Platform.h"
#include "views/FileViews.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>
#include <QUrl>

using namespace tfx::filepane;
using namespace tfx::platform;

void FilePane::renameSelected()
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = selectedRowIndexes(m_view->selectionModel(), ColumnName);
        if (!rows.isEmpty()) {
            index = rows.first();
        }
    }
    if (!index.isValid()) {
        return;
    }
    m_view->edit(index.sibling(index.row(), ColumnName));
}

void FilePane::createFolder()
{
    const QString name = QInputDialog::getText(
        this,
        UiText::t("New Folder", "新規フォルダ"),
        UiText::t("Name:", "名前:"),
        QLineEdit::Normal,
        placeholderText("New Folder", "新規フォルダ"));
    if (name.isEmpty()) {
        return;
    }
    // Avoid name collisions the same way new files do: an existing name gets
    // a " 2", " 3", ... suffix instead of failing. Unlike files, folder names
    // keep any "." as-is (no extension splitting).
    const QDir dir(m_currentPath);
    QString folderName = name;
    for (int i = 2; QFileInfo::exists(dir.filePath(folderName)); ++i) {
        folderName = QString("%1 %2").arg(name).arg(i);
    }
    if (!dir.mkdir(folderName)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create folder.", "フォルダを作成できませんでした。"));
        return;
    }
    setCurrentIndexForPath(dir.filePath(folderName));
    updateStatusLine();
}

void FilePane::createFile()
{
    const QString path = uniqueChildPath(placeholderText("New File.txt", "新規ファイル.txt"));
    QFile file(path);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create file.", "ファイルを作成できませんでした。"));
        return;
    }
    file.close();
    setCurrentIndexForPath(path);
    updateStatusLine();
}

void FilePane::moveSelectedToTrash()
{
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) {
        return;
    }
    if (QMessageBox::question(
            this,
            UiText::t("Move to Trash", "ゴミ箱へ移動"),
            UiText::t("Move selected item(s) to trash?", "選択項目をゴミ箱へ移動しますか?")) != QMessageBox::Yes) {
        return;
    }
    QStringList paths;
    for (const QUrl &url : urls) {
        paths << url.toLocalFile();
    }
    moveToTrash(paths);
    updateStatusLine();
}

QString FilePane::uniqueChildPath(const QString &baseName) const
{
    QString path = QDir(m_currentPath).filePath(baseName);
    if (!QFileInfo::exists(path)) {
        return path;
    }
    const QFileInfo info(path);
    for (int i = 2; ; ++i) {
        const QString numberedName = info.suffix().isEmpty()
            ? QString("%1 %2").arg(info.completeBaseName()).arg(i)
            : QString("%1 %2.%3").arg(info.completeBaseName()).arg(i).arg(info.suffix());
        const QString candidate = QDir(m_currentPath).filePath(numberedName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
}

QString FilePane::placeholderText(const QString &english, const QString &japanese) const
{
    return placeholderName(m_placeholderLanguage, english, japanese);
}
