#include "FilePane.h"
#include "UiText.h"
#include "controllers/GitStatusController.h"
#include "models/FileColumns.h"
#include "views/FileViews.h"

#include <QDir>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QListView>
#include <QSet>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QUrl>

QList<QUrl> FilePane::selectedUrls() const
{
    QList<QUrl> urls;
    QSet<QString> seen;
    QModelIndexList rows = selectedRowIndexes(m_view->selectionModel(), ColumnName);
    if (rows.isEmpty() && m_view->currentIndex().isValid()) {
        rows << m_view->currentIndex().sibling(m_view->currentIndex().row(), ColumnName);
    }
    for (const QModelIndex &index : rows) {
        const QString path = m_model->filePath(m_proxyModel->mapToSource(index));
        if (!seen.contains(path)) {
            urls.append(QUrl::fromLocalFile(path));
            seen.insert(path);
        }
    }
    return urls;
}

void FilePane::normalizeRowSelection()
{
    // Selection ranges lose the proxy's synthesised columns on model layout
    // changes, leaving rows partially selected. Re-select the full rows so
    // cell-level checks (Qt's ExtendedSelection press handling, context-menu
    // hit tests) keep seeing the whole multi-selection.
    QItemSelectionModel *selection = m_view->selectionModel();
    if (!selection) {
        return;
    }
    const QModelIndexList rows = selectedRowIndexes(selection, ColumnName);
    if (rows.isEmpty()) {
        return;
    }
    QItemSelection full;
    for (const QModelIndex &row : rows) {
        full.merge(rowSelection(row), QItemSelectionModel::Select);
    }
    if (full != selection->selection()) {
        selection->select(full, QItemSelectionModel::ClearAndSelect);
    }
}

QStringList FilePane::selectedLocalPaths() const
{
    QStringList paths;
    for (const QUrl &url : selectedUrls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }
    return paths;
}

void FilePane::setOpenWithApplications(const QHash<QString, QString> &applications)
{
    m_openWithApplications = applications;
}

void FilePane::setPlaceholderLanguage(const QString &language)
{
    if (language == "en" || language == "ja" || language == "auto") {
        m_placeholderLanguage = language;
    }
}

void FilePane::setShowHiddenFiles(bool show)
{
    m_showHiddenFiles = show;
    QDir::Filters filters = QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files;
    if (show) {
        filters |= QDir::Hidden | QDir::System;
    }
    m_model->setFilter(filters);
    updateStatusLine();
}

void FilePane::setPathFilter(const QString &text)
{
    // Retained for compatibility; live filtering is no longer used. Searching is
    // started explicitly via startSearch() (Enter in the search box).
    Q_UNUSED(text);
}

void FilePane::setViewMode(bool iconMode)
{
    m_iconMode = iconMode;
    if (m_viewStack->currentWidget() != m_searchView) {
        m_viewStack->setCurrentWidget(iconMode ? static_cast<QWidget *>(m_iconView)
                                               : static_cast<QWidget *>(m_view));
    }
}

bool FilePane::acceptsDropOnRow(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return false;
    }
    const QModelIndex source = m_proxyModel->mapToSource(proxyIndex.sibling(proxyIndex.row(), ColumnName));
    return m_model->fileInfo(source).isDir();
}

QString FilePane::dropDestinationDirectory(const QModelIndex &proxyIndex) const
{
    if (!acceptsDropOnRow(proxyIndex)) {
        return m_currentPath;
    }
    const QModelIndex source = m_proxyModel->mapToSource(proxyIndex.sibling(proxyIndex.row(), ColumnName));
    const QFileInfo info = m_model->fileInfo(source);
    // The parent row is a real directory entry ("<dir>/.."); resolve it so the
    // drop and the label both name the folder above rather than that path.
    const QString resolved = QDir(info.absoluteFilePath()).canonicalPath();
    return resolved.isEmpty() ? info.absoluteFilePath() : resolved;
}

QString FilePane::displayNameForDirectory(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? path : name;
}

void FilePane::setRowColors(const QString &selectedBackground, const QString &hoverBackground,
                            const QString &selectedForeground)
{
    const QList<QAbstractItemView *> views = {m_view, m_iconView, m_searchView};
    for (QAbstractItemView *view : views) {
        if (!view) {
            continue;
        }
        // FileItemDelegate has no Q_OBJECT macro, so qobject_cast is unavailable.
        if (auto *delegate = dynamic_cast<FileItemDelegate *>(view->itemDelegate())) {
            delegate->selectedBackground = QColor(selectedBackground);
            delegate->hoverBackground = QColor(hoverBackground);
            delegate->selectedForeground = QColor(selectedForeground);
            view->viewport()->update();
        }
    }
}

void FilePane::setActive(bool active)
{
    m_isActive = active;
    setProperty("activePane", active);
    style()->unpolish(this);
    style()->polish(this);
    m_badgeLabel->setProperty("activePane", active);
    m_badgeLabel->style()->unpolish(m_badgeLabel);
    m_badgeLabel->style()->polish(m_badgeLabel);
    m_pathEdit->setProperty("activePane", active);
    m_pathEdit->style()->unpolish(m_pathEdit);
    m_pathEdit->style()->polish(m_pathEdit);
    m_statusLabel->setProperty("activePane", active);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    if (m_titleBar) {
        m_titleBar->setProperty("activePane", active);
        m_titleBar->style()->unpolish(m_titleBar);
        m_titleBar->style()->polish(m_titleBar);
    }
}

void FilePane::setFileListFont(const QFont &font)
{
    // A real widget font (not a stylesheet rule) keeps the elision metrics in
    // sync with the painted glyphs; see MainWindowTheme.
    m_view->setFont(font);
    if (m_iconView) {
        m_iconView->setFont(font);
    }
    if (m_searchView) {
        m_searchView->setFont(font);
    }
    if (m_zipView) {
        m_zipView->setFont(font);
    }
}

void FilePane::setThemeColors(const QString &fileForeground, const QString &directoryForeground)
{
    m_fileForeground = fileForeground;
    m_directoryForeground = directoryForeground;
    m_proxyModel->setThemeColors(fileForeground, directoryForeground);
}

void FilePane::setGitStatusColors(const QHash<QString, QString> &labelColors)
{
    m_proxyModel->setGitColors(labelColors);
}

void FilePane::setDropTargetColor(const QString &color)
{
    const QColor parsed(color);
    if (!parsed.isValid()) {
        return;
    }
    static_cast<FileTableView *>(m_view)->dropTargetColor = parsed;
    if (m_iconView) {
        static_cast<FileIconView *>(m_iconView)->dropTargetColor = parsed;
    }
}

QModelIndex FilePane::currentSourceIndex() const
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = selectedRowIndexes(m_view->selectionModel(), ColumnName);
        if (!rows.isEmpty()) {
            index = rows.first();
        }
    }
    return index.isValid() ? m_proxyModel->mapToSource(index.sibling(index.row(), ColumnName)) : QModelIndex();
}

QFileInfo FilePane::currentFileInfo() const
{
    const QModelIndex index = currentSourceIndex();
    return index.isValid() ? m_model->fileInfo(index) : QFileInfo();
}

void FilePane::refreshGitStatuses()
{
    m_gitController->refresh(m_currentPath);
}

void FilePane::updatePreviewFromSelection()
{
    const QModelIndexList rows = selectedRowIndexes(m_view->selectionModel(), ColumnName);
    if (rows.size() > 1) {
        QStringList paths;
        for (const QModelIndex &index : rows) {
            const QModelIndex source = m_proxyModel->mapToSource(index.sibling(index.row(), 0));
            const QString path = m_model->filePath(source);
            if (!path.isEmpty()) {
                paths << path;
            }
        }
        if (paths.size() > 1) {
            emit multiSelectionPreviewRequested(paths);
            return;
        }
    }

    const QFileInfo info = currentFileInfo();
    if (info.exists()) {
        emit selectionPreviewRequested(info.absoluteFilePath());
    } else {
        emit selectionPreviewRequested(m_currentPath);
    }
}

void FilePane::updateStatusLine()
{
    if (m_viewStack && m_viewStack->currentWidget() == m_searchView) {
        const int total = m_searchModel ? m_searchModel->rowCount() : 0;
        int selected = m_searchView->selectionModel()->selectedRows().size();
        if (selected == 0 && m_searchView->currentIndex().isValid()) {
            selected = 1;
        }
        const QString selectedText = selected > 0
            ? UiText::t("%1 selected", "%1 件選択").arg(selected)
            : UiText::t("No selection", "選択なし");
        const bool searchRunning = m_searchIterator || !m_searchPendingDirs.isEmpty();
        const QString searchText = searchRunning
            ? UiText::t("Searching \"%1\"", "\"%1\" を検索中").arg(m_searchTerm)
            : UiText::t("Search \"%1\"", "\"%1\" の検索結果").arg(m_searchTerm);
        m_statusLabel->setText(
            UiText::t(" %1 matches  |  %2  |  %3 ", " %1 件一致  |  %2  |  %3 ")
                .arg(total)
                .arg(selectedText)
                .arg(searchText));
        return;
    }

    const QModelIndex root = m_view->rootIndex();
    const int total = m_proxyModel->rowCount(root);
    int selected = selectedRowIndexes(m_view->selectionModel()).size();
    if (selected == 0 && m_view->currentIndex().isValid()) {
        selected = 1;
    }
    QString selectedText = selected > 0
        ? UiText::t("%1 selected", "%1 件選択").arg(selected)
        : UiText::t("No selection", "選択なし");
    QString text = UiText::t(" %1 of %2 items  |  %3 ", " %1 / %2 件  |  %3 ")
        .arg(qMin(total, qMax(0, m_view->currentIndex().row() + 1)))
        .arg(total)
        .arg(selectedText);
    if (!m_gitBranch.isEmpty()) {
        text += QString("  |  ⎇ %1 ").arg(m_gitBranch);
    }
    m_statusLabel->setText(text);
}
