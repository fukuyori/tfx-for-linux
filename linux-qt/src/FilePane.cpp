#include "FilePane.h"
#include "UiText.h"
#include "core/FileOperations.h"
#include "core/FileTypeInfo.h"
#include "core/GitService.h"
#include "controllers/GitStatusController.h"
#include "platform/Platform.h"
#include "views/FileViews.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QDirIterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QKeyEvent>
#include <QHash>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QUrl>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QListView>
#include <QListWidget>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

using namespace tfx::core;
using namespace tfx::platform;

namespace {
int defaultColumnWidth(int column)
{
    switch (column) {
    case ColumnName:
        return 340;
    case ColumnType:
        return 120;
    case ColumnSize:
        return 96;
    case ColumnCreated:
        return 160;
    case ColumnModified:
        return 160;
    case ColumnMode:
        return 116;
    case ColumnGit:
        return 28;
    default:
        return 120;
    }
}

bool runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory, QString *errorText)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (errorText) {
            *errorText = process.errorString();
        }
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return true;
    }
    if (errorText) {
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        *errorText = stderrText.isEmpty() ? process.errorString() : stderrText;
    }
    return false;
}
}

FilePane::FilePane(const QString &label, const QString &initialPath, QWidget *parent)
    : QWidget(parent),
      m_model(new QFileSystemModel(this)),
      m_proxyModel(new FileSystemProxyModel(this)),
      m_tabBar(new QTabBar(this)),
      m_view(new FileTableView(this)),
      m_badgeLabel(new QLabel(this)),
      m_pathEdit(new QLineEdit(this)),
      m_statusLabel(new QLabel(this)),
      m_label(label)
{
    setObjectName("filePane");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(220);
    m_model->setRootPath("/");
    m_model->setFilter(QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files);
    m_model->setReadOnly(false);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);

    m_badgeLabel->setObjectName("paneBadge");
    m_pathEdit->setObjectName("panePath");
    m_statusLabel->setObjectName("paneStatus");
    m_badgeLabel->setText(m_label.toUpper());
    m_pathEdit->setFrame(false);
    m_pathEdit->setClearButtonEnabled(false);
    m_pathEdit->setPlaceholderText("/");
    m_pathEdit->installEventFilter(this);
    m_tabBar->setObjectName("paneTabs");
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->addTab(tabTitleForPath(initialPath));
    m_tabBar->setTabData(0, QFileInfo(initialPath).absoluteFilePath());

    m_view->setModel(m_proxyModel);
    m_view->setObjectName("fileTable");
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->viewport()->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropMode(QAbstractItemView::DragDrop);
    m_view->setDefaultDropAction(Qt::MoveAction);
    static_cast<FileTableView *>(m_view)->dropHandler =
        [this](const QList<QUrl> &urls, Qt::DropAction action, const QModelIndex &target) {
            QString targetDir = m_currentPath;
            if (target.isValid()) {
                const QModelIndex source = m_proxyModel->mapToSource(target.sibling(target.row(), 0));
                const QFileInfo info = m_model->fileInfo(source);
                if (info.isDir()) {
                    targetDir = info.absoluteFilePath();
                }
            }
            performDrop(urls, action, targetDir);
        };
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setShowGrid(false);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_view->setIconSize(QSize(18, 18));
    m_view->setItemDelegate(new FileItemDelegate(m_view));
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setMinimumSectionSize(24);
    m_view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->horizontalHeader()->setSectionsMovable(true);
    m_view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->horizontalHeader()->resizeSection(column, defaultColumnWidth(column));
    }
    m_view->verticalHeader()->hide();
    m_view->verticalHeader()->setDefaultSectionSize(26);
    m_view->setAlternatingRowColors(false);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->installEventFilter(this);
    applySharedColumnLayout();
    // QHeaderView resets the width of the synthesised extra columns
    // (modified/mode/git) to the default section size whenever the model is
    // reloaded or re-sorted, so the shared layout must be re-applied after
    // those events. The header's own reset slot runs before ours (it connects
    // first), so a synchronous re-apply wins.
    // While the model reorganises (sort/reload), QHeaderView resets the
    // synthesised extra columns to the default size and — in a shown window —
    // emits sectionResized for them. Those are not user actions, so saving is
    // suppressed for the whole layout-change window and the saved layout is
    // re-applied once it settles.
    const auto beginLayoutChange = [this]() { m_suppressColumnSave = true; };
    const auto endLayoutChange = [this]() {
        applySharedColumnLayout();
        QTimer::singleShot(0, this, [this]() { m_suppressColumnSave = false; });
    };
    connect(m_proxyModel, &QAbstractItemModel::layoutAboutToBeChanged, this, beginLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::modelAboutToBeReset, this, beginLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this, endLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this, endLayoutChange);
    connect(m_model, &QFileSystemModel::directoryLoaded, this, [this](const QString &) {
        applySharedColumnLayout();
    });

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

    // Auto-refresh: the file list itself is kept current by QFileSystemModel's
    // own watcher; this keeps the Git badges fresh when the directory changes,
    // plus a slow poll to catch git index changes (commits/staging).
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

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(m_badgeLabel);
    headerLayout->addWidget(m_pathEdit, 1);

    m_searchModel = new QStandardItemModel(this);
    m_searchModel->setHorizontalHeaderLabels({
        columnTitle(ColumnName),
        columnTitle(ColumnType),
        columnTitle(ColumnSize),
        columnTitle(ColumnModified),
        columnTitle(ColumnMode),
    });
    m_searchView = new QTableView(this);
    m_searchView->setObjectName("fileTable");
    m_searchView->setModel(m_searchModel);
    m_searchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_searchView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_searchView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchView->setShowGrid(false);
    m_searchView->setFrameShape(QFrame::NoFrame);
    m_searchView->setIconSize(QSize(18, 18));
    m_searchView->setItemDelegate(new FileItemDelegate(m_searchView));
    m_searchView->verticalHeader()->hide();
    m_searchView->verticalHeader()->setDefaultSectionSize(26);
    m_searchView->horizontalHeader()->setStretchLastSection(true);
    m_searchView->setColumnWidth(0, 320);
    m_searchView->setColumnWidth(1, 120);
    m_searchView->setColumnWidth(2, 96);
    m_searchView->setColumnWidth(3, 160);

    m_iconView = new FileIconView(this);
    m_iconView->setObjectName("fileIcons");
    m_iconView->setModel(m_proxyModel);
    m_iconView->setSelectionModel(m_view->selectionModel());
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setWrapping(true);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setIconSize(QSize(48, 48));
    m_iconView->setGridSize(QSize(104, 80));
    m_iconView->setWordWrap(true);
    m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_iconView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_iconView->setFrameShape(QFrame::NoFrame);
    m_iconView->setDragEnabled(true);
    m_iconView->setAcceptDrops(true);
    m_iconView->viewport()->setAcceptDrops(true);
    m_iconView->setDropIndicatorShown(true);
    m_iconView->setDragDropMode(QAbstractItemView::DragDrop);
    m_iconView->setDefaultDropAction(Qt::MoveAction);
    m_iconView->installEventFilter(this);
    static_cast<FileIconView *>(m_iconView)->dropHandler =
        [this](const QList<QUrl> &urls, Qt::DropAction action, const QModelIndex &target) {
            QString targetDir = m_currentPath;
            if (target.isValid()) {
                const QModelIndex source = m_proxyModel->mapToSource(target.sibling(target.row(), 0));
                const QFileInfo info = m_model->fileInfo(source);
                if (info.isDir()) {
                    targetDir = info.absoluteFilePath();
                }
            }
            performDrop(urls, action, targetDir);
        };
    connect(m_iconView, &QListView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            m_iconView->setCurrentIndex(index);
            openSelected();
        }
    });

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_view);
    m_viewStack->addWidget(m_iconView);
    m_viewStack->addWidget(m_searchView);
    m_viewStack->setCurrentWidget(m_view);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(0);
    connect(m_searchTimer, &QTimer::timeout, this, &FilePane::searchStep);
    const auto openSearchResult = [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString path = m_searchModel->item(index.row(), 0)->data(Qt::UserRole).toString();
        const QFileInfo info(path);
        if (!info.exists()) {
            return;
        }
        navigateTo(info.absolutePath());
        QTimer::singleShot(0, this, [this, path]() { setCurrentIndexForPath(path); });
    };
    connect(m_searchView, &QTableView::activated, this, openSearchResult);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addLayout(headerLayout);
    layout->addWidget(m_viewStack, 1);
    layout->addWidget(m_statusLabel);

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
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (m_tabBar->count() <= 1) {
            return;
        }
        m_tabBar->removeTab(index);
        if (m_tabBar->currentIndex() >= 0) {
            const QString path = m_tabBar->tabData(m_tabBar->currentIndex()).toString();
            if (!path.isEmpty()) {
                m_isSwitchingTabs = true;
                navigateTo(path, false);
                m_isSwitchingTabs = false;
            }
        }
    });

    navigateTo(initialPath, false);
    setActive(false);
}

FilePane::~FilePane() = default;

QString FilePane::currentPath() const
{
    return m_currentPath;
}

QStringList FilePane::tabPaths() const
{
    QStringList paths;
    for (int index = 0; index < m_tabBar->count(); ++index) {
        paths.append(m_tabBar->tabData(index).toString());
    }
    return paths;
}

int FilePane::activeTabIndex() const
{
    return m_tabBar->currentIndex();
}

void FilePane::restoreTabs(const QStringList &paths, int activeIndex)
{
    QStringList validPaths;
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir() && !validPaths.contains(path)) {
            validPaths.append(path);
        }
    }
    if (validPaths.isEmpty()) {
        return;
    }

    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(m_tabBar->count() - 1);
    }
    for (const QString &path : validPaths) {
        const int index = m_tabBar->addTab(tabTitleForPath(path));
        m_tabBar->setTabData(index, path);
    }
    const int clampedIndex = qBound(0, activeIndex, m_tabBar->count() - 1);
    m_tabBar->setCurrentIndex(clampedIndex);
    m_tabBar->blockSignals(false);

    m_isSwitchingTabs = true;
    navigateTo(m_tabBar->tabData(clampedIndex).toString(), false);
    m_isSwitchingTabs = false;
}

QList<QUrl> FilePane::selectedUrls() const
{
    QList<QUrl> urls;
    QSet<QString> seen;
    QModelIndexList rows = m_view->selectionModel()->selectedRows();
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

        auto *nameItem = new QStandardItem(m_iconProvider.icon(info), base.relativeFilePath(path));
        nameItem->setData(path, Qt::UserRole);
        nameItem->setForeground(QColor(info.isDir() ? m_directoryForeground : m_fileForeground));
        auto *typeItem = new QStandardItem(englishTypeName(info));
        auto *sizeItem = new QStandardItem(info.isDir() ? QString() : sizeString(info.size()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *modifiedItem = new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        auto *modeItem = new QStandardItem(modeString(info));
        m_searchModel->appendRow({nameItem, typeItem, sizeItem, modifiedItem, modeItem});
        ++m_searchMatches;
    }
    if (!m_searchIterator->hasNext()) {
        m_searchTimer->stop();
        delete m_searchIterator;
        m_searchIterator = nullptr;
        emit statusMessageRequested(UiText::t("Search complete: %1 matches", "検索完了: %1 件").arg(m_searchMatches));
    } else {
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
    if (m_viewStack && m_view) {
        m_viewStack->setCurrentWidget(m_iconMode ? static_cast<QWidget *>(m_iconView)
                                                 : static_cast<QWidget *>(m_view));
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
}

void FilePane::setThemeColors(const QString &fileForeground, const QString &directoryForeground)
{
    m_fileForeground = fileForeground;
    m_directoryForeground = directoryForeground;
    m_proxyModel->setThemeColors(fileForeground, directoryForeground);
}

void FilePane::navigateTo(const QString &path, bool recordHistory)
{
    QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isDir()) {
        emit statusMessageRequested(UiText::t("Cannot open directory: %1", "フォルダを開けません: %1").arg(path));
        return;
    }

    cancelSearch();

    const QString nextPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    if (recordHistory && !m_currentPath.isEmpty() && m_currentPath != nextPath) {
        pushHistory(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = nextPath;
    m_pathEdit->setText(displayPath(m_currentPath));
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
    } else {
        openPath(info.absoluteFilePath());
    }
}

void FilePane::renameSelected()
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = m_view->selectionModel()->selectedRows();
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
        UiText::t("New Folder", "新規フォルダ"));
    if (name.isEmpty()) {
        return;
    }
    QDir dir(m_currentPath);
    if (!dir.mkdir(name)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create folder.", "フォルダを作成できませんでした。"));
    }
    updateStatusLine();
}

void FilePane::createFile()
{
    const QString path = uniqueChildPath("New File.txt");
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

void FilePane::copySelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Copied selected item(s).", "選択項目をコピーしました。"));
}

void FilePane::cutSelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    mime->setData("application/x-tfx-cut", "1");
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Cut selected item(s).", "選択項目をカットしました。"));
}

void FilePane::performDrop(const QList<QUrl> &urls, Qt::DropAction action, const QString &targetDir)
{
    if (urls.isEmpty() || targetDir.isEmpty()) {
        return;
    }
    const bool move = (action == Qt::MoveAction);
    const QDir dir(targetDir);
    const QString targetCanonical = QFileInfo(targetDir).absoluteFilePath();
    bool changed = false;

    for (const QUrl &url : urls) {
        const QString source = url.toLocalFile();
        if (source.isEmpty()) {
            continue;
        }
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.exists()) {
            continue;
        }
        // Dropping into the same directory is a no-op.
        if (sourceInfo.absolutePath() == targetCanonical) {
            continue;
        }
        // Never move/copy a folder into itself or one of its descendants.
        if (sourceInfo.isDir()
            && (targetCanonical + "/").startsWith(sourceInfo.absoluteFilePath() + "/")) {
            emit statusMessageRequested(UiText::t("Cannot move a folder into itself.", "フォルダを自身の中へは移動できません。"));
            continue;
        }
        const QString destination = dir.filePath(sourceInfo.fileName());
        if (QFileInfo::exists(destination)) {
            emit statusMessageRequested(UiText::t("Skipped existing item: %1", "既存項目をスキップしました: %1").arg(sourceInfo.fileName()));
            continue;
        }

        bool ok = false;
        if (move) {
            ok = QFile::rename(source, destination);
            if (!ok && copyRecursively(source, destination)) {
                // Cross-device move: copy succeeded, remove the source.
                ok = sourceInfo.isDir() ? QDir(source).removeRecursively() : QFile::remove(source);
            }
        } else {
            ok = copyRecursively(source, destination);
        }
        if (ok) {
            changed = true;
        } else {
            emit statusMessageRequested(UiText::t("Could not transfer item: %1", "項目を移動/コピーできませんでした: %1").arg(sourceInfo.fileName()));
        }
    }

    if (changed) {
        reload();
    }
    updateStatusLine();
}

void FilePane::pasteIntoCurrentDirectory()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime->hasUrls()) {
        return;
    }

    const bool move = mime->hasFormat("application/x-tfx-cut");
    for (const QUrl &url : mime->urls()) {
        const QString source = url.toLocalFile();
        if (source.isEmpty()) {
            continue;
        }
        const QString destination = QDir(m_currentPath).filePath(QFileInfo(source).fileName());
        if (QFileInfo::exists(destination)) {
            emit statusMessageRequested(UiText::t("Skipped existing item: %1", "既存項目をスキップしました: %1").arg(destination));
            continue;
        }
        if (move) {
            QFile::rename(source, destination);
        } else if (!copyRecursively(source, destination)) {
            emit statusMessageRequested(UiText::t("Could not paste item: %1", "項目をペーストできませんでした: %1").arg(source));
        }
    }
    updateStatusLine();
}

void FilePane::copySelectedPaths()
{
    QStringList paths;
    for (const QUrl &url : selectedUrls()) {
        paths.append(url.toLocalFile());
    }
    QApplication::clipboard()->setText(paths.join('\n'));
    emit statusMessageRequested(UiText::t("Copied absolute path(s).", "絶対パスをコピーしました。"));
}

void FilePane::newTab()
{
    const int index = m_tabBar->addTab(tabTitleForPath(m_currentPath));
    m_tabBar->setTabData(index, m_currentPath);
    m_tabBar->setCurrentIndex(index);
}

void FilePane::closeCurrentTab()
{
    const int index = m_tabBar->currentIndex();
    if (index < 0 || m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->removeTab(index);
    const int nextIndex = qMin(index, m_tabBar->count() - 1);
    if (nextIndex >= 0) {
        m_tabBar->setCurrentIndex(nextIndex);
    }
}

void FilePane::nextTab()
{
    if (m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % m_tabBar->count());
}

void FilePane::previousTab()
{
    if (m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->setCurrentIndex((m_tabBar->currentIndex() - 1 + m_tabBar->count()) % m_tabBar->count());
}

bool FilePane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pathEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_pathEdit->setText(m_currentPath);
            m_pathEdit->selectAll();
            emit activated(this);
        }
        if (event->type() == QEvent::FocusOut) {
            m_pathEdit->setText(displayPath(m_currentPath));
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_pathEdit->setText(displayPath(m_currentPath));
                m_view->setFocus();
                return true;
            }
        }
    }
    if (watched == m_view || watched == m_iconView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            emit activated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Down
                && !m_view->currentIndex().isValid()
                && m_view->selectionModel()->selectedIndexes().isEmpty()) {
                if (selectParentEntry()) {
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                openSelected();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                goUp();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QModelIndex FilePane::currentSourceIndex() const
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = m_view->selectionModel()->selectedRows();
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

void FilePane::showFileContextMenu(const QPoint &point)
{
    const QModelIndex clicked = m_view->indexAt(point);
    if (clicked.isValid() && !m_view->selectionModel()->isSelected(clicked)) {
        m_view->selectRow(clicked.row());
    }

    const QFileInfo info = currentFileInfo();
    const bool hasSelection = info.exists();
    const bool isDirectory = hasSelection && info.isDir();
    const bool isZip = hasSelection && info.suffix().compare("zip", Qt::CaseInsensitive) == 0;

    QMenu menu(this);
    menu.addAction(UiText::t("Open", "開く"), this, &FilePane::openSelected)->setEnabled(hasSelection);

    if (hasSelection && !isDirectory) {
        auto *openWith = menu.addMenu(UiText::t("Open With", "このアプリケーションで開く"));
        openWith->addAction(UiText::t("Default Application", "既定のアプリケーション"), this, &FilePane::openSelected);
        openWith->addAction(UiText::t("Other...", "その他..."), this, &FilePane::openWithCustomApplication);
    }

    menu.addSeparator();
    menu.addAction(UiText::t("Move to Trash", "ゴミ箱へ移動"), this, &FilePane::moveSelectedToTrash)->setEnabled(hasSelection);

    auto *tagsMenu = menu.addMenu(UiText::t("Tags", "タグ"));
    tagsMenu->addAction(UiText::t("Add Custom Tag...", "カスタムタグを追加..."))->setEnabled(false);
    tagsMenu->addSeparator();
    tagsMenu->addAction("Red")->setEnabled(false);
    tagsMenu->addAction("Orange")->setEnabled(false);
    tagsMenu->addAction("Yellow")->setEnabled(false);
    tagsMenu->addAction("Green")->setEnabled(false);
    tagsMenu->addAction("Blue")->setEnabled(false);
    tagsMenu->addAction("Purple")->setEnabled(false);
    tagsMenu->addAction("Gray")->setEnabled(false);

    menu.addSeparator();
    menu.addAction(UiText::t("Rename", "名前を変更"), this, &FilePane::renameSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Create Link", "リンクを作成"), this, &FilePane::createLinkForSelection)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Compress to Zip", "zip に圧縮"), this, &FilePane::compressSelectedItemsToZip)->setEnabled(hasSelection);
    if (isZip) {
        menu.addAction(UiText::t("Extract Zip", "zip を展開"), this, &FilePane::extractSelectedZip);
    }
    menu.addAction(UiText::t("Copy Items", "項目をコピー"), this, &FilePane::copySelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Cut Items", "項目をカット"), this, &FilePane::cutSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasUrls());

    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, &FilePane::revealSelectionInFileManager)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, &FilePane::copySelectedPaths)->setEnabled(hasSelection);

    if (isDirectory) {
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, info]() {
            emit pinFolderRequested(info.absoluteFilePath());
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, &FilePane::openTerminalHere);
    }

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showEmptyAreaContextMenu(const QPoint &point)
{
    QMenu menu(this);
    menu.addAction(UiText::t("New Folder", "新規フォルダ"), this, &FilePane::createFolder);
    menu.addAction(UiText::t("New File", "新規ファイル"), this, &FilePane::createFile);
    menu.addSeparator();
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasUrls());
    menu.addSeparator();
    menu.addAction(UiText::t("Select All", "すべて選択"), this, &FilePane::selectAllVisibleItems)
        ->setEnabled(m_proxyModel->rowCount(m_view->rootIndex()) > 0);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [this]() {
        revealInFileManager(m_currentPath);
    });
    menu.addAction(UiText::t("Copy Current Path", "現在のパスをコピー"), this, [this]() {
        QApplication::clipboard()->setText(m_currentPath);
    });
    menu.addSeparator();
    menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this]() {
        emit pinFolderRequested(m_currentPath);
    });
    menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this]() {
        emit openTerminalHereRequested(m_currentPath);
    });

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showColumnContextMenu(const QPoint &point)
{
    QMenu menu(this);
    for (int column = 0; column < kColumnCount; ++column) {
        QAction *action = menu.addAction(columnTitle(column));
        action->setCheckable(true);
        action->setChecked(!m_view->isColumnHidden(column));
        action->setEnabled(column != 0);
        connect(action, &QAction::toggled, this, [this, column](bool visible) {
            m_view->setColumnHidden(column, !visible);
            saveColumnSettings();
        });
    }

    menu.addSeparator();
    menu.addAction(UiText::t("Column Settings...", "表示項目設定..."), this, &FilePane::showColumnSettingsDialog);
    menu.addAction(UiText::t("Reset Columns", "カラムをリセット"), this, &FilePane::resetColumns);

    menu.exec(m_view->horizontalHeader()->mapToGlobal(point));
}

QString FilePane::columnTitle(int column) const
{
    switch (column) {
    case ColumnName:
        return UiText::t("Name", "名前");
    case ColumnType:
        return UiText::t("Type", "種類");
    case ColumnSize:
        return UiText::t("Size", "サイズ");
    case ColumnCreated:
        return UiText::t("Date Created", "作成日時");
    case ColumnModified:
        return UiText::t("Date Modified", "更新日時");
    case ColumnMode:
        return UiText::t("File Mode", "ファイルモード");
    case ColumnGit:
        return UiText::t("Git Status", "Git ステータス");
    default:
        return {};
    }
}

void FilePane::resetColumns()
{
    const QSignalBlocker headerBlocker(m_view->horizontalHeader());
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        m_view->horizontalHeader()->moveSection(m_view->horizontalHeader()->visualIndex(column), column);
        m_view->horizontalHeader()->resizeSection(column, defaultColumnWidth(column));
    }
    saveColumnSettings();
}

void FilePane::showColumnSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(UiText::t("File List Settings", "ファイル一覧設定"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(UiText::t("Columns", "表示項目"), &dialog);
    title->setObjectName("sectionLabel");
    layout->addWidget(title);

    auto *list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    const auto addColumnItem = [this, list](int logical, bool visible) {
        auto *item = new QListWidgetItem(columnTitle(logical), list);
        item->setData(Qt::UserRole, logical);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        if (logical == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
    };
    const auto populateFromHeader = [this, list, addColumnItem]() {
        list->clear();
        for (int visual = 0; visual < kColumnCount; ++visual) {
            const int logical = m_view->horizontalHeader()->logicalIndex(visual);
            addColumnItem(logical, !m_view->isColumnHidden(logical));
        }
    };
    const auto populateDefaults = [list, addColumnItem]() {
        list->clear();
        for (int logical = 0; logical < kColumnCount; ++logical) {
            addColumnItem(logical, true);
        }
    };
    populateFromHeader();
    layout->addWidget(list);

    auto *moveLayout = new QHBoxLayout();
    auto *upButton = new QPushButton(UiText::t("Up", "上へ"), &dialog);
    auto *downButton = new QPushButton(UiText::t("Down", "下へ"), &dialog);
    auto *resetButton = new QPushButton(UiText::t("Reset", "リセット"), &dialog);
    moveLayout->addWidget(upButton);
    moveLayout->addWidget(downButton);
    moveLayout->addStretch(1);
    moveLayout->addWidget(resetButton);
    layout->addLayout(moveLayout);

    connect(upButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row > 0) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row - 1, item);
            list->setCurrentRow(row - 1);
        }
    });
    connect(downButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row >= 0 && row < list->count() - 1) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row + 1, item);
            list->setCurrentRow(row + 1);
        }
    });
    connect(resetButton, &QPushButton::clicked, &dialog, [populateDefaults]() {
        populateDefaults();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    for (int visual = 0; visual < list->count(); ++visual) {
        const int logical = list->item(visual)->data(Qt::UserRole).toInt();
        const int currentVisual = m_view->horizontalHeader()->visualIndex(logical);
        if (currentVisual != visual) {
            m_view->horizontalHeader()->moveSection(currentVisual, visual);
        }
        m_view->setColumnHidden(logical, logical == 0 ? false : list->item(visual)->checkState() != Qt::Checked);
    }
    saveColumnSettings();
}

void FilePane::revealSelectionInFileManager()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    revealInFileManager(info.absoluteFilePath());
}

void FilePane::createLinkForSelection()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    const QString linkPath = uniqueChildPath(info.fileName() + UiText::t(" link", " のリンク"));
    if (QFile::link(info.absoluteFilePath(), linkPath)) {
        QTimer::singleShot(0, this, [this, linkPath]() {
            setCurrentIndexForPath(linkPath);
        });
    } else {
        emit statusMessageRequested(UiText::t("Could not create link.", "リンクを作成できませんでした。"));
    }
}

void FilePane::openWithCustomApplication()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }

    // Prefer the desktop's native "Open With" application chooser.
    if (openWithChooser(info.absoluteFilePath())) {
        return;
    }

    // Fallback: ask for a launch command when no native chooser is available.
    bool ok = false;
    const QString command = QInputDialog::getText(this,
        UiText::t("Open With", "このアプリケーションで開く"),
        UiText::t("Application command:", "アプリケーションのコマンド:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || command.isEmpty()) {
        return;
    }
    if (!QProcess::startDetached(command, {info.absoluteFilePath()})) {
        emit statusMessageRequested(UiText::t("Could not launch: %1", "起動できませんでした: %1").arg(command));
    }
}

void FilePane::openTerminalHere()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    emit openTerminalHereRequested(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
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
        : "Archive.zip";
    const QString archivePath = uniquePathInDirectory(m_currentPath, archiveBaseName);

    QStringList arguments;
    arguments << "-r" << archivePath;
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
    if (!QDir().mkpath(destinationPath)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create extraction folder.", "展開先フォルダを作成できませんでした。"));
        return;
    }

    QString errorText;
    const QStringList arguments = { archiveInfo.absoluteFilePath(), "-d", destinationPath };
    if (!runProcess(unzipProgram, arguments, m_currentPath, &errorText)) {
        QDir(destinationPath).removeRecursively();
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

void FilePane::selectAllVisibleItems()
{
    m_view->selectAll();
    updateStatusLine();
}

void FilePane::applyDefaultColumns()
{
    auto *header = m_view->horizontalHeader();
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        header->moveSection(header->visualIndex(column), column);
        header->resizeSection(column, defaultColumnWidth(column));
    }
}

void FilePane::applySharedColumnLayout()
{
    // Column layout (order/visibility/width) is shared between panes: only the
    // left pane persists it and the right pane mirrors the left, so a single
    // (non per-pane) settings group is used. "V2" retires older layouts.
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");
    const bool hasSaved = settings.contains("width0");

    auto *header = m_view->horizontalHeader();
    const QSignalBlocker headerBlocker(header);

    if (!hasSaved) {
        settings.endGroup();
        applyDefaultColumns();
        return;
    }

    const QStringList order = settings.value("order").toStringList();
    if (order.size() == kColumnCount) {
        for (int visual = 0; visual < kColumnCount; ++visual) {
            bool ok = false;
            const int logical = order.at(visual).toInt(&ok);
            if (ok && logical >= 0 && logical < kColumnCount) {
                const int current = header->visualIndex(logical);
                if (current >= 0 && current != visual) {
                    header->moveSection(current, visual);
                }
            }
        }
    }

    for (int column = 0; column < kColumnCount; ++column) {
        const bool visible = settings.value(QString("visible%1").arg(column), true).toBool();
        m_view->setColumnHidden(column, column == ColumnName ? false : !visible);
        const int width = settings.value(QString("width%1").arg(column), defaultColumnWidth(column)).toInt();
        header->resizeSection(column, std::max(24, width));
    }
    settings.endGroup();
}

void FilePane::saveColumnSettings()
{
    // Ignore section changes triggered by the model reorganising itself rather
    // than by the user (those would persist bogus reset widths).
    if (m_suppressColumnSave) {
        return;
    }
    // Only the left pane owns the shared column layout.
    if (m_label != "LEFT") {
        return;
    }
    auto *header = m_view->horizontalHeader();
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");

    QStringList order;
    order.reserve(kColumnCount);
    for (int visual = 0; visual < kColumnCount; ++visual) {
        order.append(QString::number(header->logicalIndex(visual)));
    }
    settings.setValue("order", order);

    for (int column = 0; column < kColumnCount; ++column) {
        settings.setValue(QString("visible%1").arg(column), !m_view->isColumnHidden(column));
        settings.setValue(QString("width%1").arg(column), header->sectionSize(column));
    }
    settings.endGroup();
}

void FilePane::refreshGitStatuses()
{
    m_gitController->refresh(m_currentPath);
}

void FilePane::updatePreviewFromSelection()
{
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
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
    const QModelIndex root = m_view->rootIndex();
    const int total = m_proxyModel->rowCount(root);
    int selected = m_view->selectionModel()->selectedRows().size();
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

void FilePane::commitPathEditor()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_pathEdit->setText(displayPath(m_currentPath));
        return;
    }
    const QString expandedPath = path == "~"
        ? QDir::homePath()
        : (path.startsWith("~/") ? QDir::home().filePath(path.mid(2)) : path);
    navigateTo(expandedPath);
    m_pathEdit->setText(displayPath(m_currentPath));
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

QString FilePane::tabTitleForPath(const QString &path) const
{
    const QFileInfo info(path);
    const QString title = info.fileName();
    if (!title.isEmpty()) {
        return title;
    }
    return path == "/" ? "/" : path;
}

void FilePane::updateCurrentTabPath(const QString &path)
{
    int index = m_tabBar->currentIndex();
    if (index < 0) {
        index = m_tabBar->addTab(tabTitleForPath(path));
        m_tabBar->setCurrentIndex(index);
    }
    m_tabBar->setTabText(index, tabTitleForPath(path));
    m_tabBar->setTabData(index, path);
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
