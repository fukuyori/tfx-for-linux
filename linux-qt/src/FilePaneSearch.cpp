#include "FilePane.h"
#include "FilePaneSearchRoles.h"
#include "UiText.h"
#include "core/FileTypeInfo.h"
#include "platform/Platform.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QListView>
#include <QMenu>
#include <QSet>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>

using namespace tfx::core;
using namespace tfx::filepane;
using namespace tfx::platform;

void FilePane::startSearch(const QString &term)
{
    cancelSearch();
    const QString trimmed = term.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_searchTerm = trimmed;
    m_searchMatches = 0;
    m_searchModel->removeRows(0, m_searchModel->rowCount());

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (m_showHiddenFiles) {
        filters |= QDir::Hidden | QDir::System;
    }
    m_searchIterator = new QDirIterator(m_currentPath, filters, QDirIterator::Subdirectories);
    m_viewStack->setCurrentWidget(m_searchView);
    updateStatusLine();
    emit statusMessageRequested(UiText::t("Searching: %1...", "検索中: %1...").arg(m_searchTerm));
    m_searchTimer->start();
}

void FilePane::searchStep()
{
    if (!m_searchIterator) {
        m_searchTimer->stop();
        return;
    }
    const QDir base(m_currentPath);
    int budget = 400;
    while (budget-- > 0 && m_searchIterator->hasNext()) {
        const QString path = m_searchIterator->next();
        if (!m_searchIterator->fileName().contains(m_searchTerm, Qt::CaseInsensitive)) {
            continue;
        }
        const QFileInfo info = m_searchIterator->fileInfo();
        const QString relativePath = base.relativeFilePath(path);
        const QString typeName = englishTypeName(info);
        const QString mode = modeString(info);

        auto *nameItem = new QStandardItem(m_iconProvider.icon(info), relativePath);
        nameItem->setData(path, SearchPathRole);
        nameItem->setData(relativePath.toCaseFolded(), SearchSortRole);
        nameItem->setForeground(QColor(info.isDir() ? m_directoryForeground : m_fileForeground));
        auto *typeItem = new QStandardItem(typeName);
        typeItem->setData(typeName.toCaseFolded(), SearchSortRole);
        auto *sizeItem = new QStandardItem(info.isDir() ? QString() : sizeString(info.size()));
        sizeItem->setData(info.isDir() ? -1 : info.size(), SearchSortRole);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *modifiedItem = new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        modifiedItem->setData(info.lastModified().toMSecsSinceEpoch(), SearchSortRole);
        auto *modeItem = new QStandardItem(mode);
        modeItem->setData(mode, SearchSortRole);
        m_searchModel->appendRow({nameItem, typeItem, sizeItem, modifiedItem, modeItem});
        ++m_searchMatches;
    }
    if (!m_searchIterator->hasNext()) {
        m_searchTimer->stop();
        delete m_searchIterator;
        m_searchIterator = nullptr;
        updateStatusLine();
        emit statusMessageRequested(UiText::t("Search complete: %1 matches", "検索完了: %1 件").arg(m_searchMatches));
    } else {
        updateStatusLine();
        emit statusMessageRequested(UiText::t("Searching: %1 matches", "検索中: %1 件").arg(m_searchMatches));
    }
}

void FilePane::cancelSearch()
{
    if (m_searchTimer) {
        m_searchTimer->stop();
    }
    if (m_searchIterator) {
        delete m_searchIterator;
        m_searchIterator = nullptr;
    }
    if (m_searchModel) {
        m_searchModel->removeRows(0, m_searchModel->rowCount());
    }
    m_searchTerm.clear();
    if (m_viewStack && m_view) {
        m_viewStack->setCurrentWidget(m_iconMode ? static_cast<QWidget *>(m_iconView)
                                                 : static_cast<QWidget *>(m_view));
    }
    updateStatusLine();
}

QString FilePane::searchResultPath(const QModelIndex &index) const
{
    if (!index.isValid() || !m_searchModel) {
        return QString();
    }
    QStandardItem *item = m_searchModel->item(index.row(), 0);
    return item ? item->data(SearchPathRole).toString() : QString();
}

QStringList FilePane::selectedSearchResultPaths() const
{
    QStringList paths;
    QSet<QString> seen;
    if (!m_searchView) {
        return paths;
    }
    QModelIndexList rows = m_searchView->selectionModel()->selectedRows();
    if (rows.isEmpty() && m_searchView->currentIndex().isValid()) {
        rows << m_searchView->currentIndex();
    }
    for (const QModelIndex &index : rows) {
        const QString path = searchResultPath(index);
        if (!path.isEmpty() && !seen.contains(path)) {
            paths << path;
            seen.insert(path);
        }
    }
    return paths;
}

void FilePane::showSearchContextMenu(const QPoint &point)
{
    const QModelIndex clicked = m_searchView->indexAt(point);
    if (clicked.isValid() && !m_searchView->selectionModel()->isSelected(clicked)) {
        m_searchView->selectRow(clicked.row());
    }

    const QStringList paths = selectedSearchResultPaths();
    const bool hasSelection = !paths.isEmpty();
    const QString firstPath = hasSelection ? paths.first() : QString();
    const QFileInfo firstInfo(firstPath);

    QMenu menu(this);
    menu.addAction(UiText::t("Open", "開く"), this, [this, clicked]() {
        QModelIndex index = clicked.isValid() ? clicked : m_searchView->currentIndex();
        if (index.isValid()) {
            emit m_searchView->activated(index);
        }
    })->setEnabled(hasSelection);
    menu.addAction(UiText::t("Go to Containing Folder", "含まれるフォルダへ移動"), this, [this, firstInfo]() {
        if (!firstInfo.exists()) {
            return;
        }
        const QString path = firstInfo.absoluteFilePath();
        navigateTo(firstInfo.absolutePath());
        QTimer::singleShot(0, this, [this, path]() { setCurrentIndexForPath(path); });
    })->setEnabled(hasSelection);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [firstPath]() {
        revealInFileManager(firstPath);
    })->setEnabled(hasSelection);
    menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [paths]() {
        QApplication::clipboard()->setText(paths.join('\n'));
    })->setEnabled(hasSelection);

    menu.exec(m_searchView->viewport()->mapToGlobal(point));
}

void FilePane::updatePreviewFromSearchSelection()
{
    if (!m_searchView || !m_searchModel) {
        return;
    }

    const QModelIndexList rows = m_searchView->selectionModel()->selectedRows();
    if (rows.size() > 1) {
        QStringList paths;
        for (const QModelIndex &index : rows) {
            const QString path = m_searchModel->item(index.row(), 0)->data(SearchPathRole).toString();
            if (QFileInfo::exists(path)) {
                paths << path;
            }
        }
        if (paths.size() > 1) {
            emit multiSelectionPreviewRequested(paths);
            return;
        }
    }

    QModelIndex index = m_searchView->currentIndex();
    if (!index.isValid() && !rows.isEmpty()) {
        index = rows.first();
    }
    if (!index.isValid()) {
        emit selectionPreviewRequested(m_currentPath);
        return;
    }

    const QString path = m_searchModel->item(index.row(), 0)->data(SearchPathRole).toString();
    emit selectionPreviewRequested(QFileInfo::exists(path) ? path : m_currentPath);
}
