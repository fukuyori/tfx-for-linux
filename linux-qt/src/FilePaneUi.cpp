#include "FilePane.h"
#include "FilePaneColumns.h"
#include "FilePaneSearchRoles.h"
#include "UiText.h"
#include "core/TabState.h"
#include "models/FileColumns.h"
#include "platform/Platform.h"
#include "views/FileViews.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

using namespace tfx::core;
using namespace tfx::filepane;

void FilePane::setupPaneChrome(const QString &initialPath)
{
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
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString initialTabPath = normalizedTabPath(initialPath);
    m_tabBar->addTab(tabTitleForPath(initialPath));
    m_tabBar->setTabData(0, initialTabPath);
    m_tabBar->setTabToolTip(0, initialTabPath);
    updateTabCloseButtons();
}

QWidget *FilePane::createHeaderLayout()
{
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(m_badgeLabel);
    headerLayout->addWidget(m_pathEdit, 1);

    // Wrap the header row in a styled container so [colors] titleBar* can paint
    // the pane title bar background and track the active/inactive pane state.
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("paneTitleBar");
    m_titleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBar->setLayout(headerLayout);
    return m_titleBar;
}

void FilePane::setupFileView()
{
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
    m_view->horizontalHeader()->setSortIndicatorShown(true);
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
}

void FilePane::setupSearchView()
{
    m_searchModel = new QStandardItemModel(this);
    m_searchModel->setHorizontalHeaderLabels({
        columnTitle(ColumnName),
        columnTitle(ColumnType),
        columnTitle(ColumnSize),
        columnTitle(ColumnModified),
        columnTitle(ColumnMode),
    });
    m_searchModel->setSortRole(SearchSortRole);

    m_searchView = new QTableView(this);
    m_searchView->setObjectName("fileTable");
    m_searchView->setModel(m_searchModel);
    m_searchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_searchView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_searchView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchView->setSortingEnabled(true);
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
    m_searchView->sortByColumn(0, Qt::AscendingOrder);
    m_searchView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_searchView->installEventFilter(this);
}

void FilePane::setupIconView()
{
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
}

void FilePane::setupZipView()
{
    m_zipModel = new QStandardItemModel(this);
    m_zipModel->setHorizontalHeaderLabels({columnTitle(ColumnName), columnTitle(ColumnType)});

    m_zipView = new QTableView(this);
    m_zipView->setObjectName("fileTable");
    m_zipView->setModel(m_zipModel);
    m_zipView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_zipView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_zipView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_zipView->setShowGrid(false);
    m_zipView->setFrameShape(QFrame::NoFrame);
    m_zipView->setIconSize(QSize(18, 18));
    m_zipView->setItemDelegate(new FileItemDelegate(m_zipView));
    m_zipView->verticalHeader()->hide();
    m_zipView->verticalHeader()->setDefaultSectionSize(26);
    m_zipView->horizontalHeader()->setStretchLastSection(true);
    m_zipView->setColumnWidth(0, 360);
    connect(m_zipView, &QTableView::activated, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString entry = m_zipModel->item(index.row(), 0)->data(Qt::UserRole).toString();
        const bool isDir = m_zipModel->item(index.row(), 0)->data(Qt::UserRole + 1).toBool();
        if (entry == "..") {
            if (m_zipDir.isEmpty()) {
                exitZipMode();
            } else {
                const int slash = m_zipDir.lastIndexOf('/', m_zipDir.size() - 2);
                m_zipDir = slash >= 0 ? m_zipDir.left(slash + 1) : QString();
                populateZipView();
            }
        } else if (isDir) {
            m_zipDir = entry;
            populateZipView();
        } else if (m_zipSymlinkEntries.contains(entry)) {
            // Opening would extract the link and launch whatever it points at.
            emit statusMessageRequested(
                UiText::t("Entry is a symbolic link and was not opened: %1",
                          "エントリはシンボリックリンクのため開きませんでした: %1").arg(entry));
        } else {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(false);
            if (tempDir.isValid()
                && tfx::platform::extractZipEntry(m_zipPath, entry, tempDir.path())) {
                tfx::platform::openPath(QDir(tempDir.path()).filePath(entry));
            } else {
                emit statusMessageRequested(UiText::t("Could not extract entry.", "エントリを展開できませんでした。"));
            }
        }
    });
}

void FilePane::setupViewStack()
{
    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_view);
    m_viewStack->addWidget(m_iconView);
    m_viewStack->addWidget(m_searchView);
    m_viewStack->addWidget(m_zipView);
    m_viewStack->setCurrentWidget(m_view);
}
