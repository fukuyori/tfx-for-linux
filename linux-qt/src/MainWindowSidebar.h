#pragma once

#include <QListWidget>
#include <QTreeView>
#include <QUrl>

#include <functional>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QPaintEvent;
class QTimer;

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
