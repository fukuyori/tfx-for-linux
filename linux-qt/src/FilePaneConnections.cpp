#include "FilePane.h"
#include "FilePaneSearchRoles.h"
#include "controllers/GitStatusController.h"
#include "views/FileViews.h"

#include <QApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTableView>
#include <QTimer>

void FilePane::setupGitRefresh()
{
    m_gitController = new GitStatusController(this);
    connect(m_gitController, &GitStatusController::statusesReady,
            this, [this](const QHash<QString, QString> &statuses) {
        m_proxyModel->setGitStatuses(statuses);
    });
    connect(m_gitController, &GitStatusController::branchChanged, this, [this](const QString &branch) {
        if (branch != m_gitBranch) {
            m_gitBranch = branch;
            updateStatusLine();
        }
    });

    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(200);
    connect(m_refreshDebounce, &QTimer::timeout, this, [this]() { refreshGitStatuses(); });

    m_dirWatcher = new QFileSystemWatcher(this);
    connect(m_dirWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        m_refreshDebounce->start();
    });

    auto *gitPoll = new QTimer(this);
    gitPoll->setInterval(30000);
    connect(gitPoll, &QTimer::timeout, this, [this]() {
        if (isVisible() && !m_currentPath.isEmpty()) {
            refreshGitStatuses();
        }
    });
    gitPoll->start();
}

void FilePane::setupSearchConnections()
{
    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(0);
    connect(m_searchTimer, &QTimer::timeout, this, &FilePane::searchStep);

    const auto openSearchResult = [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString path = m_searchModel->item(index.row(), 0)->data(tfx::filepane::SearchPathRole).toString();
        const QFileInfo info(path);
        if (!info.exists()) {
            return;
        }
        navigateTo(info.absolutePath());
        QTimer::singleShot(0, this, [this, path]() { setCurrentIndexForPath(path); });
    };
    connect(m_searchView, &QTableView::activated, this, openSearchResult);
    connect(m_searchView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
                updatePreviewFromSearchSelection();
                updateStatusLine();
            });
    connect(m_searchView, &QWidget::customContextMenuRequested,
            this, &FilePane::showSearchContextMenu);
}

void FilePane::setupFileViewConnections()
{
    connect(m_view, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        if (selectionDebugEnabled()) {
            selectionDebugLog(QString("[selection] clicked index=(%1,%2)")
                .arg(index.row())
                .arg(index.column()));
        }
        QTimer::singleShot(0, this, [this, index]() {
            if (!index.isValid()) {
                return;
            }
            emit activated(this);
            m_view->setFocus(Qt::MouseFocusReason);
            // Don't collapse a Ctrl/Shift multi-selection back to a single row.
            if (!(QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                m_view->setCurrentIndex(index);
                m_view->selectionModel()->select(rowSelection(index), QItemSelectionModel::ClearAndSelect);
                m_view->setProperty("currentSelectionRow", index.row());
                m_view->viewport()->update();
            }
            updatePreviewFromSelection();
            updateStatusLine();
            logSelectionState("after deferred clicked", m_view);
        });
    });

    connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        m_view->setCurrentIndex(index);
        openSelected();
    });

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &FilePane::commitPathEditor);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
                logSelectionState("selectionChanged", m_view);
                updatePreviewFromSelection();
                updateStatusLine();
            });
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex &previous) {
                if (current.isValid()) {
                    m_view->setProperty("currentSelectionRow", current.row());
                    m_view->viewport()->update();
                    updatePreviewFromSelection();
                    updateStatusLine();
                }
                if (!selectionDebugEnabled()) {
                    return;
                }
                selectionDebugLog(QString("[selection] currentChanged current=(%1,%2) previous=(%3,%4)")
                    .arg(current.row())
                    .arg(current.column())
                    .arg(previous.row())
                    .arg(previous.column()));
                logSelectionState("currentChanged", m_view);
            });

    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        if (m_view->indexAt(point).isValid()) {
            showFileContextMenu(point);
        } else {
            showEmptyAreaContextMenu(point);
        }
    });

    connect(m_view->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &FilePane::showColumnContextMenu);
    connect(m_view->horizontalHeader(), &QHeaderView::sectionMoved, this, [this]() {
        saveColumnSettings();
    });
    connect(m_view->horizontalHeader(), &QHeaderView::sectionResized, this, [this]() {
        saveColumnSettings();
    });
    connect(m_view->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this]() {
        // Sorting reaches the model through a layout change, and column saves
        // are suppressed for its duration; persist once that window closes.
        QTimer::singleShot(0, this, [this]() { saveColumnSettings(); });
    });
}

void FilePane::setupTabConnections()
{
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        const QString path = m_tabBar->tabData(index).toString();
        if (!path.isEmpty() && path != m_currentPath) {
            m_isSwitchingTabs = true;
            navigateTo(path, false);
            m_isSwitchingTabs = false;
        }
        emit tabsChanged();
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (m_tabBar->count() <= 1) {
            return;
        }
        m_tabBar->removeTab(index);
        updateTabCloseButtons();
        if (m_tabBar->currentIndex() >= 0) {
            const QString path = m_tabBar->tabData(m_tabBar->currentIndex()).toString();
            if (!path.isEmpty()) {
                m_isSwitchingTabs = true;
                navigateTo(path, false);
                m_isSwitchingTabs = false;
            }
        }
        emit tabsChanged();
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this]() {
        emit tabsChanged();
    });
    connect(m_tabBar, &QWidget::customContextMenuRequested, this, &FilePane::showTabContextMenu);
}
