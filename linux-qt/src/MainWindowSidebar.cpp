#include "MainWindow.h"
#include "views/SidebarViews.h"
#include "UiText.h"
#include "core/SidebarLogic.h"
#include "platform/Platform.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QSignalBlocker>
#include <QSocketNotifier>
#include <QStorageInfo>
#include <QThread>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>

#include <fcntl.h>

namespace {
// Pinned rows show the folder with its directory ("~"-relative under home);
// the list view middle-elides what doesn't fit.
QString pinnedDisplayPath(const QString &path)
{
    return tfx::core::pinnedDisplayPath(path, QDir::homePath());
}
}


void MainWindow::buildFolderSidebar(const QString &initialPath)
{
    m_treeModel->setRootPath("/");
    m_treeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_treeView->setObjectName("folderTree");
    m_pinnedList->setObjectName("pinnedList");
    m_treeView->setModel(m_treeModel);
    m_treeView->setRootIndex(m_treeModel->index("/"));
    for (int column = 1; column < m_treeModel->columnCount(); ++column) {
        m_treeView->hideColumn(column);
    }
    m_treeView->header()->hide();
    m_treeView->setItemDelegate(new FolderTreeDelegate(m_treeView));
    m_treeView->setCurrentIndex(m_treeModel->index(initialPath));
    m_treeView->setIndentation(10);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    // Drops onto tree nodes are accepted; tree nodes themselves are not
    // draggable. Same volume defaults to Move, across volumes to Copy, with
    // Shift forcing Move and Ctrl forcing Copy; the self/subtree guard and
    // conflict prompts run in the pane's shared drop pipeline.
    m_treeView->setDragDropMode(QAbstractItemView::DropOnly);
    m_treeView->setDragEnabled(false);
    m_treeView->setAcceptDrops(true);
    m_treeView->viewport()->setAcceptDrops(true);
    {
        auto *tree = static_cast<FolderTreeView *>(m_treeView);
        tree->dropHandler = [this](const QList<QUrl> &urls, Qt::KeyboardModifiers modifiers,
                                   const QString &targetDir) {
            Qt::DropAction action;
            if (modifiers.testFlag(Qt::ControlModifier)) {
                action = Qt::CopyAction;
            } else if (modifiers.testFlag(Qt::ShiftModifier)) {
                action = Qt::MoveAction;
            } else {
                // Volume comparison via the cached mount roots: string-only,
                // so a hung network mount cannot stall the drop.
                const QString targetRoot = diskRootForPath(targetDir);
                bool sameVolume = true;
                for (const QUrl &url : urls) {
                    const QString source = url.toLocalFile();
                    if (!source.isEmpty() && diskRootForPath(source) != targetRoot) {
                        sameVolume = false;
                        break;
                    }
                }
                action = sameVolume ? Qt::MoveAction : Qt::CopyAction;
            }
            activePane()->dropOntoDirectory(urls, action, targetDir);
        };
    }

    connect(m_treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        const QString path = m_treeModel->filePath(index);
        m_treeNavigationInProgress = true;
        activePane()->navigateTo(path);
        m_treeNavigationInProgress = false;
    });
    // Rows arrive asynchronously: each load under the pending path shifts the
    // layout, so the scroll-to-top is re-applied until the target folder's
    // own listing (triggered by the one-level expand) has arrived.
    connect(m_treeModel, &QFileSystemModel::directoryLoaded, this, [this](const QString &loaded) {
        if (m_pendingTreeScrollPath.isEmpty()) {
            return;
        }
        const QString target = m_pendingTreeScrollPath;
        const QString loadedPrefix = loaded.endsWith(QLatin1Char('/')) ? loaded : loaded + QLatin1Char('/');
        if (target != loaded && !target.startsWith(loadedPrefix)) {
            return;
        }
        const QModelIndex index = m_treeModel->index(target);
        if (!index.isValid()) {
            return;
        }
        m_treeView->scrollTo(index, QAbstractItemView::PositionAtTop);
        if (loaded == target) {
            m_pendingTreeScrollPath.clear();
        }
    });
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &point) {
        const QModelIndex index = m_treeView->indexAt(point);
        if (!index.isValid()) {
            return;
        }
        const QString path = m_treeModel->filePath(index);
        QMenu menu(this);
        menu.addAction(UiText::t("Open", "開く"), this, [this, path]() {
            m_treeNavigationInProgress = true;
            activePane()->navigateTo(path);
            m_treeNavigationInProgress = false;
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [path]() {
            tfx::platform::revealInFileManager(path);
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Collapse All", "すべて折りたたむ"), this, &MainWindow::collapseFolderTree);
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, path]() {
            addPinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->openAt(path);
            setTerminalVisible(true);
        });
        menu.exec(m_treeView->viewport()->mapToGlobal(point));
    });

    const QStringList pinnedPaths = {
        QDir::homePath(),
        QDir::home().filePath("Desktop"),
        QDir::home().filePath("Documents"),
        QDir::home().filePath("Downloads"),
        QDir::home().filePath("Pictures"),
    };
    for (const QString &path : pinnedPaths) {
        addPinnedFolder(path);
    }
    m_pinnedList->setUniformItemSizes(true);
    m_pinnedList->setTextElideMode(Qt::ElideMiddle);
    m_pinnedList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pinnedList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pinnedList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pinnedList->setDragEnabled(true);
    m_pinnedList->setAcceptDrops(true);
    m_pinnedList->viewport()->setAcceptDrops(true);
    m_pinnedList->setDropIndicatorShown(false);
    m_pinnedList->setDragDropMode(QAbstractItemView::DragDrop);
    m_pinnedList->setDefaultDropAction(Qt::MoveAction);
    auto *pinned = static_cast<PinnedListWidget *>(m_pinnedList);
    pinned->onReordered = [this]() {
        updatePinnedFolderArea();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    };
    pinned->onExternalFoldersDropped = [this](const QStringList &folders, int row) {
        int insertAt = qBound(0, row, m_pinnedList->count());
        for (const QString &path : folders) {
            const QFileInfo info(path);
            if (!info.isDir()) {
                continue;
            }
            const QString clean = info.canonicalFilePath().isEmpty()
                ? info.absoluteFilePath() : info.canonicalFilePath();
            bool exists = false;
            for (int r = 0; r < m_pinnedList->count(); ++r) {
                if (m_pinnedList->item(r)->data(Qt::UserRole).toString() == clean) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                continue;
            }
            auto *item = new QListWidgetItem(pinnedDisplayPath(clean));
            item->setSizeHint(QSize(0, 18));
            item->setData(Qt::UserRole, clean);
            item->setToolTip(clean);
            m_pinnedList->insertItem(insertAt, item);
            ++insertAt;
        }
        updatePinnedFolderArea();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    };
    connect(m_pinnedList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        activePane()->navigateTo(item->data(Qt::UserRole).toString());
    });
    m_pinnedList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pinnedList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QListWidgetItem *item = m_pinnedList->itemAt(point);
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        QMenu menu(this);
        menu.addAction(UiText::t("Open", "開く"), this, [this, path]() {
            activePane()->navigateTo(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [path]() {
            tfx::platform::revealInFileManager(path);
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Unpin Folder", "ピン留めを解除"), this, [this, path]() {
            removePinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->openAt(path);
            setTerminalVisible(true);
        });
        menu.exec(m_pinnedList->viewport()->mapToGlobal(point));
    });

    m_diskList = new QListWidget(m_sidebar);
    m_diskList->setObjectName("diskList");
    m_diskList->setUniformItemSizes(true);
    m_diskList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_diskList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_diskList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_diskList->setItemDelegate(new DiskListDelegate(m_diskList));
    connect(m_diskList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        activePane()->navigateTo(item->data(Qt::UserRole).toString());
    });

    // /proc/self/mounts raises POLLPRI (Exception) when the mount table
    // changes, so USB plug/unplug refreshes the list without polling.
    m_mountsDebounce = new QTimer(this);
    m_mountsDebounce->setSingleShot(true);
    m_mountsDebounce->setInterval(300);
    connect(m_mountsDebounce, &QTimer::timeout, this, [this]() {
        refreshDiskList();
    });
    m_mountsFd = ::open("/proc/self/mounts", O_RDONLY);
    if (m_mountsFd >= 0) {
        m_mountsNotifier = new QSocketNotifier(m_mountsFd, QSocketNotifier::Exception, this);
        connect(m_mountsNotifier, &QSocketNotifier::activated, this, [this]() {
            m_mountsDebounce->start();
        });
    }
    refreshDiskList();
}

// Lists every mounted browsable volume: the root filesystem, real block
// devices (skipping loop-mounted squashfs like snaps and /boot partitions),
// and network mounts. The scan runs on a worker thread because statfs on an
// unresponsive network mount can block for minutes; a scan superseded by a
// newer one is discarded.
void MainWindow::refreshDiskList()
{
    const int generation = ++m_diskScanGeneration;
    QPointer<MainWindow> self(this);
    QThread *worker = QThread::create([self, generation]() {
        QList<DiskEntry> entries;
        const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
        for (const QStorageInfo &volume : volumes) {
            if (!volume.isValid() || !volume.isReady()) {
                continue;
            }
            const QString root = volume.rootPath();
            if (!tfx::core::shouldListVolume(root,
                                             QString::fromUtf8(volume.device()),
                                             QString::fromUtf8(volume.fileSystemType()))) {
                continue;
            }
            DiskEntry entry;
            entry.root = root;
            entry.title = volume.name();
            if (entry.title.isEmpty()) {
                entry.title = root == QLatin1String("/") ? QStringLiteral("/")
                                                         : QFileInfo(root).fileName();
            }
            const qint64 total = volume.bytesTotal();
            const qint64 free = volume.bytesAvailable();
            if (total > 0) {
                entry.usage = 1.0 - static_cast<double>(free) / static_cast<double>(total);
            }
            entry.tooltip = UiText::t("%1\n%2 free of %3", "%1\n空き %2 / %3")
                                .arg(root,
                                     QLocale().formattedDataSize(free),
                                     QLocale().formattedDataSize(total));
            entries.append(entry);
        }
        QMetaObject::invokeMethod(qApp, [self, generation, entries]() {
            if (self && generation == self->m_diskScanGeneration) {
                self->applyDiskList(entries);
            }
        }, Qt::QueuedConnection);
    });
    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::applyDiskList(const QList<DiskEntry> &entries)
{
    const QString previous = m_diskList->currentItem()
        ? m_diskList->currentItem()->data(Qt::UserRole).toString()
        : QString();
    m_diskList->clear();
    m_diskRoots.clear();
    for (const DiskEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.title);
        item->setData(Qt::UserRole, entry.root);
        if (entry.usage >= 0.0) {
            item->setData(kDiskUsageRole, entry.usage);
        }
        item->setToolTip(entry.tooltip);
        m_diskList->addItem(item);
        m_diskRoots.append(entry.root);
    }
    const int rowHeight = 24;
    const int frameHeight = m_diskList->frameWidth() * 2;
    m_diskList->setFixedHeight(m_diskList->count() > 0
                                   ? rowHeight * m_diskList->count() + frameHeight + 2
                                   : 0);
    updateDiskSelection(!previous.isEmpty() ? previous : activePane()->currentPath());
    applySidebarSectionStates();
}

// Longest known mount root containing `path` — pure string matching, so an
// unresponsive mount can never block the UI here.
QString MainWindow::diskRootForPath(const QString &path) const
{
    QString best;
    for (const QString &root : m_diskRoots) {
        const bool contains = root == QLatin1String("/")
            || path == root
            || path.startsWith(root + QLatin1Char('/'));
        if (contains && root.size() > best.size()) {
            best = root;
        }
    }
    return best;
}

// Highlights the volume containing `path` without navigating.
void MainWindow::updateDiskSelection(const QString &path)
{
    if (!m_diskList || path.isEmpty()) {
        return;
    }
    const QString root = diskRootForPath(path);
    const QSignalBlocker blocker(m_diskList);
    for (int row = 0; row < m_diskList->count(); ++row) {
        QListWidgetItem *item = m_diskList->item(row);
        if (item->data(Qt::UserRole).toString() == root) {
            m_diskList->setCurrentItem(item);
            return;
        }
    }
    m_diskList->setCurrentItem(nullptr);
}

void MainWindow::applySidebarSectionStates()
{
    if (!m_pinnedHeader || !m_diskHeader || !m_treeHeader) {
        return;
    }
    const auto headerText = [](bool collapsed, const QString &title) {
        return QString::fromUtf8(collapsed ? "▸ " : "▾ ") + title;
    };
    m_pinnedHeader->setText(headerText(m_pinnedCollapsed, UiText::t("Pinned", "ピン留め")));
    m_diskHeader->setText(headerText(m_disksCollapsed, UiText::t("Disks", "ディスク")));
    m_treeHeader->setText(headerText(m_foldersCollapsed, UiText::t("Folders", "フォルダー")));
    updatePinnedFolderArea();
    m_diskList->setVisible(!m_disksCollapsed && m_diskList->count() > 0);
    m_treeView->setVisible(!m_foldersCollapsed);
}

void MainWindow::addPinnedFolder(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isDir()) {
        return;
    }
    const QString cleanPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        if (m_pinnedList->item(row)->data(Qt::UserRole).toString() == cleanPath) {
            return;
        }
    }

    auto *item = new QListWidgetItem(pinnedDisplayPath(cleanPath));
    item->setSizeHint(QSize(0, 18));
    item->setData(Qt::UserRole, cleanPath);
    item->setToolTip(cleanPath);
    m_pinnedList->addItem(item);
    updatePinnedFolderArea();
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::removePinnedFolder(const QString &path)
{
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        QListWidgetItem *item = m_pinnedList->item(row);
        if (item->data(Qt::UserRole).toString() == path) {
            delete m_pinnedList->takeItem(row);
            updatePinnedFolderArea();
            if (!m_isRestoringSettings) {
                saveSettings();
            }
            return;
        }
    }
}

// Mirrors the folder tree to the pane's current directory. Navigation
// arriving from outside the tree (file pane, pinned folders, path bar)
// collapses branches off the new path, expands the target node one level so
// the listed subfolders are visible, and scrolls it to the top of the
// viewport. Clicks inside the tree keep the tree's own scroll position.
void MainWindow::syncFolderTree(const QString &path)
{
    const QModelIndex index = m_treeModel->index(path);
    if (!index.isValid()) {
        return;
    }
    if (m_treeNavigationInProgress) {
        m_pendingTreeScrollPath.clear();
        m_treeView->setCurrentIndex(index);
        return;
    }
    collapseTreeBranchesOffPath(m_treeView->rootIndex(), m_treeModel->filePath(index));
    m_treeView->setCurrentIndex(index);
    m_treeView->expand(index);
    m_treeView->scrollTo(index, QAbstractItemView::PositionAtTop);
    // Ancestor listings may still be loading; directoryLoaded re-applies the
    // scroll until the layout is final (see buildFolderSidebar).
    m_pendingTreeScrollPath = m_treeModel->filePath(index);
}

void MainWindow::collapseTreeBranchesOffPath(const QModelIndex &parent, const QString &targetPath)
{
    const int rows = m_treeModel->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = m_treeModel->index(row, 0, parent);
        if (!m_treeView->isExpanded(child)) {
            continue;
        }
        const QString childPath = m_treeModel->filePath(child);
        const QString childPrefix = childPath.endsWith(QLatin1Char('/'))
            ? childPath
            : childPath + QLatin1Char('/');
        if (targetPath == childPath || targetPath.startsWith(childPrefix)) {
            collapseTreeBranchesOffPath(child, targetPath);
        } else {
            m_treeView->collapse(child);
        }
    }
}

void MainWindow::collapseFolderTree()
{
    m_treeView->collapseAll();
    const QModelIndex current = m_treeModel->index(activePane()->currentPath());
    if (current.isValid()) {
        m_treeView->setCurrentIndex(current);
        m_treeView->scrollTo(current, QAbstractItemView::PositionAtCenter);
    }
}

void MainWindow::updatePinnedFolderArea()
{
    const int pinnedCount = m_pinnedList->count();
    const int rowHeight = 18;
    const int frameHeight = m_pinnedList->frameWidth() * 2;
    const int listHeight = pinnedCount > 0 ? (rowHeight * pinnedCount) + frameHeight + 2 : 0;
    m_pinnedList->setFixedHeight(listHeight);
    m_pinnedList->setVisible(pinnedCount > 0);

    const int spacerHeight = pinnedCount > 0 ? 6 : 4;
    m_pinnedSpacer->setFixedHeight(spacerHeight);
    if (m_pinnedCollapsed) {
        m_pinnedList->setVisible(false);
    }
}
