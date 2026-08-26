#include "FilePane.h"
#include "UiText.h"
#include "core/TabState.h"
#include "platform/Platform.h"
#include "views/BreadcrumbBar.h"
#include "views/FileViews.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QListView>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>

using namespace tfx::core;

QString FilePane::currentPath() const
{
    return m_currentPath;
}

void FilePane::navigateTo(const QString &path, bool recordHistory)
{
    QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isDir()) {
        emit statusMessageRequested(UiText::t("Cannot open directory: %1", "フォルダを開けません: %1").arg(path));
        return;
    }

    cancelSearch();
    resetTypeAhead();
    m_zipPath.clear();
    m_zipDir.clear();
    m_zipEntries.clear();

    const QString nextPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    if (recordHistory && !m_currentPath.isEmpty() && m_currentPath != nextPath) {
        pushHistory(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = nextPath;
    m_pathEdit->setText(displayPath(m_currentPath));
    if (m_breadcrumb) {
        m_breadcrumb->setPath(m_currentPath);
    }
    if (!m_isSwitchingTabs) {
        updateCurrentTabPath(m_currentPath);
    }
    const QModelIndex navRoot = m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath));
    m_view->setRootIndex(navRoot);
    m_iconView->setRootIndex(navRoot);
    m_view->setProperty("currentSelectionRow", -1);
    if (m_dirWatcher) {
        if (!m_dirWatcher->directories().isEmpty()) {
            m_dirWatcher->removePaths(m_dirWatcher->directories());
        }
        if (!m_currentPath.isEmpty()) {
            m_dirWatcher->addPath(m_currentPath);
        }
    }
    refreshGitStatuses();
    updateStatusLine();
    emit directoryChanged(m_currentPath);
    emit activated(this);
}

void FilePane::focusFileList()
{
    QWidget *target = m_view;
    if (m_viewStack && m_viewStack->currentWidget() == m_iconView) {
        target = m_iconView;
    }
    target->setFocus(Qt::OtherFocusReason);
}

void FilePane::goUp()
{
    const QString previousPath = m_currentPath;
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath());
        QTimer::singleShot(0, this, [this, previousPath]() {
            setCurrentIndexForPath(previousPath);
        });
    }
}

void FilePane::clearHistory()
{
    m_backStack.clear();
    m_forwardStack.clear();
}

void FilePane::goBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }
    m_forwardStack.push(m_currentPath);
    navigateTo(m_backStack.pop(), false);
}

void FilePane::goForward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }
    m_backStack.push(m_currentPath);
    navigateTo(m_forwardStack.pop(), false);
}

void FilePane::reload()
{
    m_model->setRootPath(QString());
    const QModelIndex reloadRoot = m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath));
    m_view->setRootIndex(reloadRoot);
    m_iconView->setRootIndex(reloadRoot);
    m_view->setProperty("currentSelectionRow", -1);
    refreshGitStatuses();
    updateStatusLine();
}

void FilePane::openSelected()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    if (info.isDir()) {
        navigateTo(info.absoluteFilePath());
        QTimer::singleShot(0, this, [this]() {
            selectParentEntry();
        });
    } else if (info.suffix().compare("zip", Qt::CaseInsensitive) == 0) {
        openZip(info.absoluteFilePath());
    } else if (tryRunExecutable(info)) {
        // Handled as an executable (binary run directly, script prompted).
    } else {
        tfx::platform::openPath(info.absoluteFilePath());
    }
}

void FilePane::commitPathEditor()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_pathEdit->setText(displayPath(m_currentPath));
        showBreadcrumb();
        return;
    }
    const QString expandedPath = path == "~"
        ? QDir::homePath()
        : (path.startsWith("~/") ? QDir::home().filePath(path.mid(2)) : path);
    navigateTo(expandedPath);
    m_pathEdit->setText(displayPath(m_currentPath));
    showBreadcrumb();
    focusFileList();
}

void FilePane::showBreadcrumb()
{
    if (m_pathStack && m_breadcrumb) {
        m_pathStack->setCurrentWidget(m_breadcrumb);
    }
}

QString FilePane::displayPath(const QString &path) const
{
    const QString home = QDir::homePath();
    if (path == home) {
        return "~";
    }
    if (path.startsWith(home + "/")) {
        return "~" + path.mid(home.size());
    }
    return path;
}

void FilePane::pushHistory(const QString &path)
{
    if (m_backStack.isEmpty() || m_backStack.top() != path) {
        m_backStack.push(path);
    }
}

void FilePane::setCurrentIndexForPath(const QString &path)
{
    const QModelIndex index = m_proxyModel->mapFromSource(m_model->index(path));
    if (index.isValid()) {
        selectProxyIndex(index);
    }
}

void FilePane::selectProxyIndex(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    const QModelIndex nameIndex = index.sibling(index.row(), ColumnName);
    m_view->setCurrentIndex(nameIndex);
    m_view->selectionModel()->select(rowSelection(nameIndex), QItemSelectionModel::ClearAndSelect);
    m_view->setProperty("currentSelectionRow", nameIndex.row());
    m_view->scrollTo(nameIndex, QAbstractItemView::PositionAtCenter);
    m_view->viewport()->update();
    updatePreviewFromSelection();
    updateStatusLine();
}

bool FilePane::selectParentEntry()
{
    const QModelIndex root = m_view->rootIndex();
    const int rows = m_proxyModel->rowCount(root);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_proxyModel->index(row, ColumnName, root);
        if (m_proxyModel->data(index, Qt::DisplayRole).toString() == "..") {
            selectProxyIndex(index);
            return true;
        }
    }

    if (rows > 0) {
        selectProxyIndex(m_proxyModel->index(0, ColumnName, root));
        return true;
    }
    return false;
}
