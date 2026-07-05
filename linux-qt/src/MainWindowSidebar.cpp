#include "MainWindow.h"
#include "MainWindowSidebar.h"
#include "UiText.h"
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
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeView>
#include <QUrl>

class FolderTreeDelegate : public QStyledItemDelegate
{
public:
    explicit FolderTreeDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->icon = QIcon();
        option->decorationSize = QSize(0, 0);
        option->features &= ~QStyleOptionViewItem::HasDecoration;
    }
};

void PinnedListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    if (!currentItem()) {
        return;
    }
    auto *drag = new QDrag(this);
    drag->setMimeData(new QMimeData());
    drag->exec(Qt::MoveAction);
}

void PinnedListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->source() == this || event->mimeData()->hasUrls()) {
        m_dragActive = true;
        m_dropRow = dropRowAt(event->position().toPoint());
        event->acceptProposedAction();
        viewport()->update();
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void PinnedListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->source() == this || event->mimeData()->hasUrls()) {
        m_dragActive = true;
        m_dropRow = dropRowAt(event->position().toPoint());
        event->acceptProposedAction();
        viewport()->update();
        return;
    }
    QListWidget::dragMoveEvent(event);
}

void PinnedListWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dragActive = false;
    viewport()->update();
    QListWidget::dragLeaveEvent(event);
}

void PinnedListWidget::dropEvent(QDropEvent *event)
{
    const int row = dropRowAt(event->position().toPoint());
    m_dragActive = false;
    viewport()->update();

    if (event->source() == this) {
        const int from = currentRow();
        if (from >= 0) {
            int to = row;
            if (to > from) {
                to -= 1;
            }
            QListWidgetItem *moved = takeItem(from);
            insertItem(qBound(0, to, count()), moved);
            setCurrentItem(moved);
            if (onReordered) {
                onReordered();
            }
        }
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        QStringList folders;
        for (const QUrl &url : event->mimeData()->urls()) {
            const QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                folders << path;
            }
        }
        if (onExternalFoldersDropped) {
            onExternalFoldersDropped(folders, row);
        }
        event->acceptProposedAction();
        return;
    }
    QListWidget::dropEvent(event);
}

void PinnedListWidget::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);
    if (!m_dragActive) {
        return;
    }
    int y = 0;
    if (m_dropRow <= 0) {
        y = count() > 0 ? visualItemRect(item(0)).top() : 0;
    } else if (m_dropRow >= count()) {
        y = count() > 0 ? visualItemRect(item(count() - 1)).bottom() : 0;
    } else {
        y = visualItemRect(item(m_dropRow)).top();
    }
    QPainter painter(viewport());
    QColor indicator("#63F28D");
    indicator.setAlphaF(qBound(0.0, dropIndicatorOpacity, 1.0));
    painter.setPen(QPen(indicator, 2));
    painter.drawLine(2, y, viewport()->width() - 2, y);
}

int PinnedListWidget::dropRowAt(const QPoint &pos)
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        return count();
    }
    const QRect rect = visualItemRect(item(index.row()));
    return pos.y() > rect.center().y() ? index.row() + 1 : index.row();
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
    m_treeView->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_treeView->setDragEnabled(false);
    m_treeView->setAcceptDrops(false);

    connect(m_treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        const QString path = m_treeModel->filePath(index);
        activePane()->navigateTo(path);
    });
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &point) {
        const QModelIndex index = m_treeView->indexAt(point);
        if (!index.isValid()) {
            return;
        }
        const QString path = m_treeModel->filePath(index);
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
            auto *item = new QListWidgetItem(info.fileName().isEmpty() ? clean : info.fileName());
            item->setSizeHint(QSize(0, 18));
            item->setData(Qt::UserRole, clean);
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

    auto *item = new QListWidgetItem(info.fileName().isEmpty() ? cleanPath : info.fileName());
    item->setSizeHint(QSize(0, 18));
    item->setData(Qt::UserRole, cleanPath);
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
}
