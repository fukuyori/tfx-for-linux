#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;

// Clickable breadcrumb path display for the pane header. Each segment
// navigates to its ancestor folder; clicking the empty area switches the
// header to the editable path field. Long paths collapse leading segments
// behind an ellipsis.
class BreadcrumbBar : public QWidget
{
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    // Show `path` as clickable segments ("~" for the home prefix).
    void setPath(const QString &path);
    // Show a single non-navigable text (used while browsing ZIP archives).
    void setStaticText(const QString &text);

signals:
    void pathClicked(const QString &path);
    void editRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Segment
    {
        QString label;
        QString path;
    };

    void rebuild();
    void updateElision();

    QList<Segment> m_segments;
    QString m_staticText;
    QHBoxLayout *m_layout = nullptr;
    QList<QWidget *> m_segmentWidgets; // buttons and separators, in order
    QList<int> m_segmentStarts;        // widget index where each segment starts
    QLabel *m_ellipsis = nullptr;
};
