#pragma once

#include <QColor>
#include <QIcon>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeView>
#include <QUrl>

#include <functional>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QPaintEvent;
class QTimer;

// Item data role carrying a DISKS row's 0.0-1.0 usage ratio.
inline constexpr int kDiskUsageRole = Qt::UserRole + 1;

// Folder tree accepting drops of file URLs onto its nodes. The hovered row is
// highlighted without being selected (selection would navigate the pane), and
// a collapsed node auto-expands after a short hover. Tree nodes themselves
// are not draggable.
class FolderTreeView : public QTreeView
{
public:
    explicit FolderTreeView(QWidget *parent = nullptr);

    // Invoked on a drop: (urls, keyboard modifiers at drop, target directory).
    std::function<void(const QList<QUrl> &, Qt::KeyboardModifiers, const QString &)> dropHandler;

    // Highlight colour for the hovered drop target.
    QColor dropTargetColor{QStringLiteral("#63F28D")};

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString directoryForIndex(const QModelIndex &index) const;
    void setHoverIndex(const QModelIndex &index);
    void clearDropState();

    QPersistentModelIndex m_hoverIndex;
    QTimer *m_expandTimer = nullptr;
    bool m_dragActive = false;
};

class PinnedListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

    std::function<void(const QStringList &folders, int row)> onExternalFoldersDropped;
    std::function<void()> onReordered;

    // Opacity of the drop insertion indicator ([opacity] dropIndicator).
    double dropIndicatorOpacity = 0.85;
    // Insertion-line colour, kept in step with the other views' drop accent
    // ([colors] fileListRowDropTarget).
    QColor dropTargetColor{QStringLiteral("#63F28D")};

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    int dropRowAt(const QPoint &pos);

    bool m_dragActive = false;
    int m_dropRow = 0;
};

// Disk row: name text with a thin usage bar along the bottom edge.
class DiskListDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(24);
        return size;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem adjusted(option);
        adjusted.rect.setBottom(adjusted.rect.bottom() - 5);
        QStyledItemDelegate::paint(painter, adjusted, index);

        const double usage = index.data(kDiskUsageRole).toDouble();
        const QRect rect = option.rect;
        const QRect track(rect.left() + 4, rect.bottom() - 4, rect.width() - 8, 3);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#2A333A"));
        painter->drawRect(track);
        QRect fill = track;
        fill.setWidth(qMax(1, static_cast<int>(track.width() * qBound(0.0, usage, 1.0))));
        painter->setBrush(usage > 0.9 ? QColor("#C0605E") : QColor("#5E7A8C"));
        painter->drawRect(fill);
        painter->restore();
    }
};

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
