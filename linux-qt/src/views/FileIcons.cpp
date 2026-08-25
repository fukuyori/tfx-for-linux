#include "views/FileIcons.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace tfx::views {
namespace {

// The shapes follow prism-fm's 24x24 icon grid, so they line up with the rest
// of that design language.
constexpr qreal kGrid = 24.0;

QPainterPath folderPath()
{
    // prism-fm: M10 4H4a2 2 0 00-2 2v12a2 2 0 002 2h16a2 2 0 002-2V8a2 2 0 00-2-2h-8l-2-2z
    // The closing edge runs diagonally from the body back up to the tab.
    QPainterPath path;
    // The tab is deepened slightly against the raw path: at a 20px render the
    // original 2-unit step all but disappears, and the glyph reads as a plain
    // rectangle rather than a folder.
    path.moveTo(11, 4);
    path.lineTo(4.5, 4);
    path.quadTo(2, 4, 2, 6.5);
    path.lineTo(2, 17.5);
    path.quadTo(2, 20, 4.5, 20);
    path.lineTo(19.5, 20);
    path.quadTo(22, 20, 22, 17.5);
    path.lineTo(22, 9.5);
    path.quadTo(22, 7, 19.5, 7);
    path.lineTo(13, 7);
    path.closeSubpath();
    return path;
}

QPainterPath pagePath()
{
    // prism-fm: M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z
    // The closing edge is the folded corner, from (20,8) back up to (14,2).
    QPainterPath path;
    path.moveTo(14, 2);
    path.lineTo(6.5, 2);
    path.quadTo(4, 2, 4, 4.5);
    path.lineTo(4, 19.5);
    path.quadTo(4, 22, 6.5, 22);
    path.lineTo(17.5, 22);
    path.quadTo(20, 22, 20, 19.5);
    path.lineTo(20, 8);
    path.closeSubpath();
    return path;
}

QPixmap renderIcon(const QColor &color, bool folder, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / kGrid, size / kGrid);

    if (folder) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(folderPath());
        return pixmap;
    }

    // Outlined like prism-fm's file glyph, with the fold drawn on top.
    QPen pen(color, 1.7);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(pagePath());
    painter.drawPolyline(QPolygonF({QPointF(14, 2), QPointF(14, 8), QPointF(20, 8)}));
    return pixmap;
}

QIcon buildIcon(const QColor &color, bool folder)
{
    QIcon icon;
    // A handful of sizes so the views get a crisp pixmap instead of a scaled one.
    for (const int size : {16, 20, 24, 32, 48, 64}) {
        icon.addPixmap(renderIcon(color, folder, size));
    }
    return icon;
}

QIcon cached(const QColor &color, bool folder)
{
    // Keyed by colour: the palette changes on a config reload, not per row.
    static QHash<QString, QIcon> cache;
    const QString key = QString("%1|%2").arg(color.name(QColor::HexArgb)).arg(folder ? 1 : 0);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const QIcon icon = buildIcon(color, folder);
    cache.insert(key, icon);
    return icon;
}

}

QIcon folderIcon(const QColor &color)
{
    return cached(color, true);
}

QIcon fileIcon(const QColor &color)
{
    return cached(color, false);
}

}
