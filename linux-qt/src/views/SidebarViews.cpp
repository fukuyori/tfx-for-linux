#include "views/SidebarViews.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileSystemModel>
#include <QMimeData>
#include <QPainter>
#include <QTimer>

FolderTreeView::FolderTreeView(QWidget *parent)
    : QTreeView(parent)
{
    m_expandTimer = new QTimer(this);
    m_expandTimer->setSingleShot(true);
    m_expandTimer->setInterval(600);
    connect(m_expandTimer, &QTimer::timeout, this, [this]() {
        if (m_dragActive && m_hoverIndex.isValid() && !isExpanded(m_hoverIndex)) {
            expand(m_hoverIndex);
        }
    });
}

QString FolderTreeView::directoryForIndex(const QModelIndex &index) const
{
    const auto *fsModel = qobject_cast<QFileSystemModel *>(model());
    if (!fsModel || !index.isValid()) {
        return {};
    }
    return fsModel->filePath(index);
}

void FolderTreeView::setHoverIndex(const QModelIndex &index)
{
    if (m_hoverIndex == QPersistentModelIndex(index)) {
        return;
    }
    m_hoverIndex = index;
    m_expandTimer->start();
    viewport()->update();
}

void FolderTreeView::clearDropState()
{
    m_dragActive = false;
    m_hoverIndex = QPersistentModelIndex();
    m_expandTimer->stop();
    viewport()->update();
}

void FolderTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        m_dragActive = true;
        setHoverIndex(indexAt(event->position().toPoint()));
        event->acceptProposedAction();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

void FolderTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        QTreeView::dragMoveEvent(event);
        return;
    }
    const QModelIndex index = indexAt(event->position().toPoint());
    setHoverIndex(index);
    if (index.isValid()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FolderTreeView::dragLeaveEvent(QDragLeaveEvent *event)
{
    clearDropState();
    QTreeView::dragLeaveEvent(event);
}

void FolderTreeView::dropEvent(QDropEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const QString target = directoryForIndex(index);
    if (!event->mimeData()->hasUrls() || !dropHandler || target.isEmpty()) {
        clearDropState();
        event->ignore();
        return;
    }
    dropHandler(event->mimeData()->urls(), event->modifiers(), target);
    clearDropState();
    event->acceptProposedAction();
}

void FolderTreeView::paintEvent(QPaintEvent *event)
{
    QTreeView::paintEvent(event);
    if (!m_dragActive || !m_hoverIndex.isValid()) {
        return;
    }
    QRect rect = visualRect(m_hoverIndex);
    rect.setLeft(0);
    rect.setRight(viewport()->width() - 1);
    QPainter painter(viewport());
    QColor fill = dropTargetColor;
    fill.setAlpha(42);
    painter.fillRect(rect, fill);
    painter.setPen(QPen(dropTargetColor, 1));
    painter.drawRect(rect.adjusted(0, 0, -1, -1));
}

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
    QColor indicator = dropTargetColor;
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
