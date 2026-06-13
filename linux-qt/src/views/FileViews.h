#pragma once

#include "models/FileColumns.h"

#include <QApplication>
#include <QColor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QIcon>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QUrl>

#include <functional>

// Optional, env-gated selection diagnostics shared by the views and FilePane.
bool selectionDebugEnabled();
void selectionDebugLog(const QString &message);
void logSelectionState(const char *where, QTableView *view);

// Selection covering the whole row of `index` across all logical columns.
QItemSelection rowSelection(const QModelIndex &index);

// Item delegate that paints the persistent selected-row / hover highlight.
class FileItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem adjusted(option);
        initStyleOption(&adjusted, index);

        const auto *view = qobject_cast<const QTableView *>(parent());
        const QModelIndex current = view ? view->currentIndex() : QModelIndex();
        const int logicalRow = view ? view->property("currentSelectionRow").toInt() : -1;
        const bool currentRow = (logicalRow >= 0 && logicalRow == index.row())
            || (logicalRow < 0 && current.isValid() && current.row() == index.row());
        const bool selected = adjusted.state.testFlag(QStyle::State_Selected) || currentRow;
        const bool hovered = adjusted.state.testFlag(QStyle::State_MouseOver);
        if (selected || hovered) {
            painter->save();
            painter->fillRect(adjusted.rect, selected ? QColor("#31576B") : QColor("#1F2830"));
            painter->restore();
            adjusted.backgroundBrush = Qt::NoBrush;
        }
        if (selected) {
            adjusted.palette.setColor(QPalette::Text, QColor("#FFFFFF"));
            adjusted.palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
            adjusted.state &= ~QStyle::State_Selected;
        }

        QStyledItemDelegate::paint(painter, adjusted, index);
    }
};

// Details (table) view with explicit drag-start, drop routing, multi-selection
// (Shift range / Ctrl toggle), and persistent row highlight.
class FileTableView : public QTableView
{
public:
    using QTableView::QTableView;

    // Invoked on a drop of file URLs: (urls, action, target index under cursor).
    std::function<void(const QList<QUrl> &, Qt::DropAction, const QModelIndex &)> dropHandler;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QTableView::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QTableView::dragMoveEvent(event);
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasUrls() || !dropHandler) {
            QTableView::dropEvent(event);
            return;
        }
        const QModelIndex target = indexAt(event->position().toPoint());
        const Qt::DropAction action = event->modifiers().testFlag(Qt::ControlModifier)
            ? Qt::CopyAction
            : Qt::MoveAction;
        dropHandler(event->mimeData()->urls(), action, target);
        event->acceptProposedAction();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressedIndex = indexAt(event->pos());
        m_pressedModifiers = event->modifiers();
        m_pressPos = event->pos();
        QTableView::mousePressEvent(event);
        logSelectionState("after base mousePress", this);
        if (event->button() != Qt::LeftButton) {
            return;
        }

        // Keep an existing multi-selection on a plain press so it can be dragged;
        // selection collapses on release if no drag happens.
        const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
        const bool alreadySelected = selectionModel() && m_pressedIndex.isValid()
            && selectionModel()->isSelected(m_pressedIndex);
        const bool multi = selectionModel() && selectionModel()->selectedRows().size() > 1;
        if (plain && alreadySelected && multi) {
            setProperty("currentSelectionRow", m_pressedIndex.row());
            return;
        }

        applyPersistentSelection(m_pressedIndex, m_pressedModifiers);
        logSelectionState("after apply mousePress", this);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        // Start a drag explicitly: the custom selection handling above otherwise
        // leaves the view in rubber-band mode instead of initiating a drag.
        if ((event->buttons() & Qt::LeftButton) && m_pressedIndex.isValid() && model() && selectionModel()
            && (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            QMimeData *mime = model()->mimeData(selectionModel()->selectedIndexes());
            if (mime && mime->hasUrls()) {
                auto *drag = new QDrag(this);
                drag->setMimeData(mime);
                const QPixmap pixmap = dragPixmap();
                if (!pixmap.isNull()) {
                    drag->setPixmap(pixmap);
                    drag->setHotSpot(QPoint(14, 12));
                }
                drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
                return;
            }
            delete mime;
        }
        QTableView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (selectionDebugEnabled()) {
            selectionDebugLog(QString("[selection] mouseRelease pos=(%1,%2) index=(%3,%4) button=%5")
                .arg(event->pos().x())
                .arg(event->pos().y())
                .arg(indexAt(event->pos()).row())
                .arg(indexAt(event->pos()).column())
                .arg(static_cast<int>(event->button())));
        }
        QTableView::mouseReleaseEvent(event);
        logSelectionState("after base mouseRelease", this);
        if (event->button() != Qt::LeftButton) {
            return;
        }
        // For Ctrl/Shift the press already set the selection; re-applying on
        // release would toggle it back or rebuild the range. Only plain clicks
        // finalize here (collapse to the clicked row when no drag occurred).
        if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
            return;
        }

        const QPersistentModelIndex index = m_pressedIndex;
        QTimer::singleShot(0, this, [this, index]() {
            applyPersistentSelection(index, Qt::NoModifier);
            logSelectionState("after deferred mouseRelease", this);
        });
    }

private:
    QPixmap dragPixmap() const
    {
        if (!selectionModel() || !model()) {
            return {};
        }
        const QModelIndexList rows = selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            return {};
        }
        const qreal dpr = devicePixelRatioF();
        const int rowHeight = 24;
        const int maxRows = 4;
        const int shown = qMin(rows.size(), maxRows);
        const bool overflow = rows.size() > maxRows;
        const int width = 240;
        const int height = rowHeight * shown + (overflow ? 18 : 0);

        QPixmap pixmap(QSize(width, height) * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < shown; ++i) {
            const QRectF rect(0, i * rowHeight, width, rowHeight);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(38, 61, 76, 230));
            painter.drawRoundedRect(rect.adjusted(0, 1, 0, -1), 4, 4);
            const QModelIndex index = rows.at(i);
            int x = 6;
            const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
            if (!icon.isNull()) {
                icon.paint(&painter, QRect(x, i * rowHeight + 3, 18, 18));
                x += 24;
            }
            painter.setPen(QColor("#FFFFFF"));
            const QString name = index.data(Qt::DisplayRole).toString();
            const QString elided = painter.fontMetrics().elidedText(name, Qt::ElideRight, width - x - 8);
            painter.drawText(QRect(x, i * rowHeight, width - x - 8, rowHeight), Qt::AlignVCenter, elided);
        }
        if (overflow) {
            painter.setPen(QColor("#D9E1E8"));
            painter.drawText(QRect(0, rowHeight * shown, width, 18), Qt::AlignCenter,
                             QString("+%1").arg(rows.size() - maxRows));
        }
        return pixmap;
    }

    void applyPersistentSelection(const QPersistentModelIndex &persistentIndex, Qt::KeyboardModifiers modifiers)
    {
        if (!persistentIndex.isValid() || !selectionModel()) {
            return;
        }

        const QModelIndex index = persistentIndex;
        if (modifiers.testFlag(Qt::ShiftModifier) && m_anchorRow >= 0 && model()) {
            // Range select from the anchor to the clicked row.
            const QModelIndex parent = index.parent();
            const int from = qMin(m_anchorRow, index.row());
            const int to = qMax(m_anchorRow, index.row());
            const QItemSelection range(model()->index(from, 0, parent),
                                       model()->index(to, kColumnCount - 1, parent));
            selectionModel()->select(range, QItemSelectionModel::ClearAndSelect);
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        } else if (modifiers.testFlag(Qt::ControlModifier)) {
            // Toggle the clicked row, keeping the rest of the selection.
            selectionModel()->select(rowSelection(index),
                                     QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
            setCurrentIndex(index);
            m_anchorRow = index.row();
        } else {
            setCurrentIndex(index);
            selectionModel()->select(rowSelection(index),
                                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_anchorRow = index.row();
        }
        setProperty("currentSelectionRow", index.row());
        viewport()->update();
        logSelectionState("applyPersistentSelection", this);
    }

    QPersistentModelIndex m_pressedIndex;
    Qt::KeyboardModifiers m_pressedModifiers;
    QPoint m_pressPos;
    int m_anchorRow = -1;
};

// Icon-mode counterpart of FileTableView. Drag-out uses the base view's default
// handling; drops of file URLs are routed through dropHandler.
class FileIconView : public QListView
{
public:
    using QListView::QListView;

    std::function<void(const QList<QUrl> &, Qt::DropAction, const QModelIndex &)> dropHandler;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressPos = event->pos();
        m_pressValid = indexAt(event->pos()).isValid();
        QListView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if ((event->buttons() & Qt::LeftButton) && m_pressValid && model() && selectionModel()
            && (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            QMimeData *mime = model()->mimeData(selectionModel()->selectedIndexes());
            if (mime && mime->hasUrls()) {
                auto *drag = new QDrag(this);
                drag->setMimeData(mime);
                const QIcon icon = qvariant_cast<QIcon>(currentIndex().data(Qt::DecorationRole));
                if (!icon.isNull()) {
                    drag->setPixmap(icon.pixmap(48, 48));
                    drag->setHotSpot(QPoint(24, 24));
                }
                drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
                return;
            }
            delete mime;
        }
        QListView::mouseMoveEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QListView::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QListView::dragMoveEvent(event);
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasUrls() || !dropHandler) {
            QListView::dropEvent(event);
            return;
        }
        const QModelIndex target = indexAt(event->position().toPoint());
        const Qt::DropAction action = event->modifiers().testFlag(Qt::ControlModifier)
            ? Qt::CopyAction
            : Qt::MoveAction;
        dropHandler(event->mimeData()->urls(), action, target);
        event->acceptProposedAction();
    }

private:
    QPoint m_pressPos;
    bool m_pressValid = false;
};
