#pragma once

#include <QListWidget>

#include <functional>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QPaintEvent;

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
