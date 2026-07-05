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
#include <QRubberBand>
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

// One index (at `column`) per row that has any selected cell. Unlike
// QItemSelectionModel::selectedRows(), this does not require every column of
// the row to be selected: ranges covering the proxy's synthesised columns do
// not survive model layout changes (sort, refresh), which would otherwise
// make a multi-selection invisible to file operations.
QModelIndexList selectedRowIndexes(const QItemSelectionModel *selectionModel, int column = 0);

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
        // Highlight the full row even when a layout change dropped some
        // columns from the stored selection range.
        const bool rowSelected = view && view->selectionModel()
            && view->selectionModel()->rowIntersectsSelection(index.row(), index.parent());
        const bool selected = adjusted.state.testFlag(QStyle::State_Selected) || rowSelected || currentRow;
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

    // Highlight colour for the in-progress drop target ([colors] fileListRowDropTarget).
    QColor dropTargetColor{QStringLiteral("#63F28D")};

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            updateDropHighlight(event->position().toPoint());
            event->acceptProposedAction();
        } else {
            QTableView::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            updateDropHighlight(event->position().toPoint());
            event->acceptProposedAction();
        } else {
            QTableView::dragMoveEvent(event);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        clearDropHighlight();
        QTableView::dragLeaveEvent(event);
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
        clearDropHighlight();
        event->acceptProposedAction();
    }

    void paintEvent(QPaintEvent *event) override
    {
        QTableView::paintEvent(event);
        paintDropHighlight();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressedIndex = indexAt(event->pos());
        m_pressedModifiers = event->modifiers();
        m_pressPos = event->pos();

        const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
        // A row counts as selected when any of its cells is: ranges lose the
        // synthesised columns on model layout changes, and the pressed cell
        // may be one of them. Base ExtendedSelection checks the exact cell
        // and would otherwise clear the multi-selection on press, so repair
        // the rows to full-width before the base class looks at them.
        const bool rowSelected = event->button() == Qt::LeftButton
            && m_pressedIndex.isValid() && selectionModel()
            && selectionModel()->rowIntersectsSelection(m_pressedIndex.row(), m_pressedIndex.parent());
        if (rowSelected && !selectionModel()->isSelected(m_pressedIndex)) {
            QItemSelection full;
            const QModelIndexList rows = selectedRowIndexes(selectionModel());
            for (const QModelIndex &row : rows) {
                full.merge(rowSelection(row), QItemSelectionModel::Select);
            }
            selectionModel()->select(full, QItemSelectionModel::Select);
        }

        QTableView::mousePressEvent(event);
        logSelectionState("after base mousePress", this);
        if (event->button() != Qt::LeftButton) {
            return;
        }

        if (!m_pressedIndex.isValid()) {
            // Rubber-band selection from the empty area. QTableView's own
            // setSelection() gives up when a band corner falls outside the
            // rows, so the band is tracked and applied here. Ctrl/Shift keeps
            // the existing selection and adds the band to it.
            m_bandActive = true;
            m_bandOrigin = event->pos();
            m_bandBaseSelection = (plain || !selectionModel())
                ? QItemSelection()
                : selectionModel()->selection();
            return;
        }
        // Keep an existing multi-selection on a plain press so it can be dragged;
        // selection collapses on release if no drag happens.
        const bool multi = selectionModel() && selectedRowIndexes(selectionModel()).size() > 1;
        if (plain && rowSelected && multi) {
            setProperty("currentSelectionRow", m_pressedIndex.row());
            return;
        }

        applyPersistentSelection(m_pressedIndex, m_pressedModifiers);
        logSelectionState("after apply mousePress", this);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_bandActive && (event->buttons() & Qt::LeftButton)) {
            updateBandSelection(event->pos());
            return;
        }
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
        if (m_bandActive) {
            // End rubber-band selection; keep what the band selected.
            m_bandActive = false;
            m_bandBaseSelection = QItemSelection();
            if (m_rubberBand) {
                m_rubberBand->hide();
            }
            logSelectionState("after band release", this);
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
    void updateBandSelection(const QPoint &pos)
    {
        if (!model() || !selectionModel()) {
            return;
        }
        const QRect band = QRect(m_bandOrigin, pos).normalized();
        if (!m_rubberBand) {
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
        }
        m_rubberBand->setGeometry(band);
        m_rubberBand->show();

        QItemSelection selection = m_bandBaseSelection;
        const QModelIndex root = rootIndex();
        const int rowCount = model()->rowCount(root);
        if (rowCount > 0) {
            // rowAt() returns -1 in the empty area past the last row (or above
            // the first row when dragging up), so clamp the band edges to the
            // row range they crossed.
            int first = rowAt(band.top());
            int last = rowAt(band.bottom());
            if (first >= 0 || last >= 0) {
                if (first < 0) {
                    first = 0;
                }
                if (last < 0) {
                    last = rowCount - 1;
                }
                selection.merge(QItemSelection(model()->index(first, 0, root),
                                               model()->index(last, kColumnCount - 1, root)),
                                QItemSelectionModel::Select);
            }
        }
        selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    }

    void updateDropHighlight(const QPoint &pos)
    {
        m_dropTarget = indexAt(pos);
        m_dropActive = true;
        viewport()->update();
    }

    void clearDropHighlight()
    {
        if (!m_dropActive && !m_dropTarget.isValid()) {
            return;
        }
        m_dropActive = false;
        m_dropTarget = QPersistentModelIndex();
        viewport()->update();
    }

    void paintDropHighlight()
    {
        if (!m_dropActive) {
            return;
        }
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor accent = dropTargetColor;
        if (m_dropTarget.isValid()) {
            QRect rect = visualRect(m_dropTarget);
            rect.setLeft(0);
            rect.setRight(viewport()->width() - 1);
            QColor fill = accent;
            fill.setAlpha(42);
            painter.fillRect(rect, fill);
            QPen pen(accent, 2);
            painter.setPen(pen);
            painter.drawRect(rect.adjusted(1, 1, -2, -2));
            return;
        }
        QPen pen(accent, 2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(viewport()->rect().adjusted(3, 3, -4, -4), 6, 6);
    }

    QPixmap dragPixmap() const
    {
        if (!selectionModel() || !model()) {
            return {};
        }
        const QModelIndexList rows = selectedRowIndexes(selectionModel(), 0);
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
    QPersistentModelIndex m_dropTarget;
    Qt::KeyboardModifiers m_pressedModifiers;
    QPoint m_pressPos;
    int m_anchorRow = -1;
    bool m_dropActive = false;
    bool m_bandActive = false;
    QPoint m_bandOrigin;
    QItemSelection m_bandBaseSelection;
    QRubberBand *m_rubberBand = nullptr;
};

// Icon-mode counterpart of FileTableView. Drag-out uses the base view's default
// handling; drops of file URLs are routed through dropHandler.
class FileIconView : public QListView
{
public:
    using QListView::QListView;

    std::function<void(const QList<QUrl> &, Qt::DropAction, const QModelIndex &)> dropHandler;

    // Highlight colour for the in-progress drop target ([colors] fileListRowDropTarget).
    QColor dropTargetColor{QStringLiteral("#63F28D")};

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
            updateDropHighlight(event->position().toPoint());
            event->acceptProposedAction();
        } else {
            QListView::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            updateDropHighlight(event->position().toPoint());
            event->acceptProposedAction();
        } else {
            QListView::dragMoveEvent(event);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        clearDropHighlight();
        QListView::dragLeaveEvent(event);
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
        clearDropHighlight();
        event->acceptProposedAction();
    }

    void paintEvent(QPaintEvent *event) override
    {
        QListView::paintEvent(event);
        paintDropHighlight();
    }

private:
    void updateDropHighlight(const QPoint &pos)
    {
        m_dropTarget = indexAt(pos);
        m_dropActive = true;
        viewport()->update();
    }

    void clearDropHighlight()
    {
        if (!m_dropActive && !m_dropTarget.isValid()) {
            return;
        }
        m_dropActive = false;
        m_dropTarget = QPersistentModelIndex();
        viewport()->update();
    }

    void paintDropHighlight()
    {
        if (!m_dropActive) {
            return;
        }
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor accent = dropTargetColor;
        if (m_dropTarget.isValid()) {
            const QRect rect = visualRect(m_dropTarget).adjusted(2, 2, -3, -3);
            QColor fill = accent;
            fill.setAlpha(38);
            painter.fillRect(rect, fill);
            painter.setPen(QPen(accent, 2));
            painter.drawRoundedRect(rect, 5, 5);
            return;
        }
        QPen pen(accent, 2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(viewport()->rect().adjusted(3, 3, -4, -4), 6, 6);
    }

    QPoint m_pressPos;
    QPersistentModelIndex m_dropTarget;
    bool m_pressValid = false;
    bool m_dropActive = false;
};
