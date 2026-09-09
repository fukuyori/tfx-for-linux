#include "views/FileIcons.h"

#include <QHash>
#include <QFileInfo>
#include <QCache>
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

QPixmap renderIcon(const QColor &color, FileIconKind kind, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / kGrid, size / kGrid);

    if (kind == FileIconKind::Folder) {
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
    const auto line = [&painter](qreal x1, qreal y1, qreal x2, qreal y2) { painter.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    const auto box = [&painter](qreal x, qreal y, qreal w, qreal h) { painter.drawRoundedRect(QRectF(x, y, w, h), 1, 1); };
    const auto triangle = [&painter](qreal x, qreal y) { painter.drawPolygon(QPolygonF{QPointF(x, y), QPointF(x, y + 8), QPointF(x + 6, y + 4)}); };
    // Independently drawn Qt glyphs using the file categories in prism-fm.
    switch (kind) {
    case FileIconKind::Image:
        box(3, 4, 18, 16); painter.drawEllipse(QPointF(8, 9), 2, 2);
        painter.drawPolyline(QPolygonF{QPointF(4, 18), QPointF(11, 12), QPointF(15, 16), QPointF(19, 11)}); break;
    case FileIconKind::Video:
        box(2, 6, 13, 12); painter.drawPolygon(QPolygonF{QPointF(15, 10), QPointF(21, 7), QPointF(21, 17), QPointF(15, 14)}); break;
    case FileIconKind::Audio:
        painter.drawEllipse(QPointF(6, 18), 3, 2); painter.drawEllipse(QPointF(17, 16), 3, 2);
        line(9, 18, 9, 5); line(9, 5, 20, 3); line(20, 3, 20, 16); break;
    case FileIconKind::Code: case FileIconKind::Markup:
        painter.drawPolyline(QPolygonF{QPointF(7, 6), QPointF(2, 12), QPointF(7, 18)});
        painter.drawPolyline(QPolygonF{QPointF(17, 6), QPointF(22, 12), QPointF(17, 18)});
        if (kind == FileIconKind::Code) line(14, 4, 10, 20); break;
    case FileIconKind::Script:
        box(2, 3, 20, 18); painter.drawPolyline(QPolygonF{QPointF(6, 8), QPointF(10, 12), QPointF(6, 16)}); line(13, 16, 18, 16); break;
    case FileIconKind::Presentation:
        box(3, 3, 18, 13); triangle(10, 5); line(12, 16, 12, 21); line(7, 21, 17, 21); break;
    case FileIconKind::Database:
        painter.drawEllipse(QRectF(4, 3, 16, 6)); line(4, 6, 4, 18); line(20, 6, 20, 18);
        painter.drawArc(QRectF(4, 9, 16, 6), 180 * 16, 180 * 16); painter.drawArc(QRectF(4, 15, 16, 6), 180 * 16, 180 * 16); break;
    case FileIconKind::Font:
        line(4, 5, 20, 5); line(4, 5, 4, 8); line(20, 5, 20, 8); line(12, 5, 12, 20); line(8, 20, 16, 20); break;
    case FileIconKind::Disk:
        painter.drawEllipse(QPointF(12, 12), 9, 9); painter.drawEllipse(QPointF(12, 12), 2, 2); line(15, 5, 19, 9); break;
    case FileIconKind::Executable:
        box(3, 3, 18, 18); triangle(9, 8); break;
    case FileIconKind::Library:
        box(4, 3, 16, 18); line(8, 3, 8, 21); line(11, 8, 17, 8); line(11, 12, 16, 12); line(8, 17, 20, 17); break;
    case FileIconKind::Key:
        painter.drawEllipse(QPointF(7, 16), 4, 4); line(10, 13, 20, 3); line(17, 6, 20, 9); line(14, 9, 16, 11); break;
    case FileIconKind::Threed:
        painter.drawPolygon(QPolygonF{QPointF(12, 2), QPointF(21, 7), QPointF(21, 17), QPointF(12, 22), QPointF(3, 17), QPointF(3, 7)});
        line(3, 7, 12, 12); line(21, 7, 12, 12); line(12, 12, 12, 22); break;
    default:
        painter.drawPath(pagePath());
        painter.drawPolyline(QPolygonF{QPointF(14, 2), QPointF(14, 8), QPointF(20, 8)});
        switch (kind) {
        case FileIconKind::Pdf:
            line(8, 19, 8, 12); box(8, 12, 5, 4); line(15, 12, 15, 19); line(15, 12, 18, 12); line(15, 15, 18, 15); break;
        case FileIconKind::Spreadsheet:
            box(7, 11, 10, 8); line(12, 11, 12, 19); line(7, 15, 17, 15); break;
        case FileIconKind::Document:
            line(7, 9, 11, 9); line(7, 13, 17, 13); line(7, 17, 17, 17); break;
        case FileIconKind::Text:
            line(7, 12, 17, 12); line(7, 16, 14, 16); break;
        case FileIconKind::Data:
            box(7, 15, 2, 4); box(11, 11, 2, 8); box(15, 13, 2, 6); break;
        case FileIconKind::Archive:
            line(11, 4, 11, 6); line(11, 8, 11, 10); line(11, 12, 11, 14); box(9, 16, 4, 3); break;
        default: break;
        }
    }
    return pixmap;
}

QIcon buildIcon(const QColor &color, FileIconKind kind)
{
    QIcon icon;
    // A handful of sizes so the views get a crisp pixmap instead of a scaled one.
    for (const int size : {16, 20, 24, 32, 48, 64}) {
        icon.addPixmap(renderIcon(color, kind, size));
    }
    return icon;
}

QIcon cached(const QColor &color, FileIconKind kind)
{
    // Keyed by colour: the palette changes on a config reload, not per row.
    static QCache<QString, QIcon> cache(256);
    const QString key = QString("%1|%2").arg(color.name(QColor::HexArgb)).arg(static_cast<int>(kind));
    if (const auto *icon = cache.object(key)) return *icon;
    const QIcon icon = buildIcon(color, kind);
    cache.insert(key, new QIcon(icon));
    return icon;
}

}

QIcon folderIcon(const QColor &color)
{
    return cached(color, FileIconKind::Folder);
}

QIcon fileIcon(const QColor &color)
{
    return cached(color, FileIconKind::File);
}

FileIconKind fileIconKind(const QString &name)
{
    static const QHash<QString, FileIconKind> types{
        {"jpg", FileIconKind::Image},
        {"jpeg", FileIconKind::Image},
        {"png", FileIconKind::Image},
        {"gif", FileIconKind::Image},
        {"webp", FileIconKind::Image},
        {"svg", FileIconKind::Image},
        {"bmp", FileIconKind::Image},
        {"ico", FileIconKind::Image},
        {"tiff", FileIconKind::Image},
        {"tif", FileIconKind::Image},
        {"raw", FileIconKind::Image},
        {"cr2", FileIconKind::Image},
        {"nef", FileIconKind::Image},
        {"heic", FileIconKind::Image},
        {"heif", FileIconKind::Image},
        {"avif", FileIconKind::Image},
        {"jxl", FileIconKind::Image},
        {"mp4", FileIconKind::Video},
        {"mkv", FileIconKind::Video},
        {"webm", FileIconKind::Video},
        {"mov", FileIconKind::Video},
        {"avi", FileIconKind::Video},
        {"wmv", FileIconKind::Video},
        {"flv", FileIconKind::Video},
        {"m4v", FileIconKind::Video},
        {"ts", FileIconKind::Video},
        {"vob", FileIconKind::Video},
        {"ogv", FileIconKind::Video},
        {"3gp", FileIconKind::Video},
        {"mp3", FileIconKind::Audio},
        {"wav", FileIconKind::Audio},
        {"ogg", FileIconKind::Audio},
        {"flac", FileIconKind::Audio},
        {"m4a", FileIconKind::Audio},
        {"aac", FileIconKind::Audio},
        {"wma", FileIconKind::Audio},
        {"opus", FileIconKind::Audio},
        {"ape", FileIconKind::Audio},
        {"alac", FileIconKind::Audio},
        {"mid", FileIconKind::Audio},
        {"midi", FileIconKind::Audio},
        {"pdf", FileIconKind::Pdf},
        {"xls", FileIconKind::Spreadsheet},
        {"xlsx", FileIconKind::Spreadsheet},
        {"ods", FileIconKind::Spreadsheet},
        {"csv", FileIconKind::Spreadsheet},
        {"tsv", FileIconKind::Spreadsheet},
        {"ppt", FileIconKind::Presentation},
        {"pptx", FileIconKind::Presentation},
        {"odp", FileIconKind::Presentation},
        {"key", FileIconKind::Presentation},
        {"doc", FileIconKind::Document},
        {"docx", FileIconKind::Document},
        {"odt", FileIconKind::Document},
        {"rtf", FileIconKind::Document},
        {"tex", FileIconKind::Document},
        {"pages", FileIconKind::Document},
        {"epub", FileIconKind::Document},
        {"txt", FileIconKind::Text},
        {"md", FileIconKind::Text},
        {"log", FileIconKind::Text},
        {"nfo", FileIconKind::Text},
        {"readme", FileIconKind::Text},
        {"changelog", FileIconKind::Text},
        {"js", FileIconKind::Code},
        {"jsx", FileIconKind::Code},
        {"tsx", FileIconKind::Code},
        {"css", FileIconKind::Code},
        {"scss", FileIconKind::Code},
        {"sass", FileIconKind::Code},
        {"less", FileIconKind::Code},
        {"html", FileIconKind::Code},
        {"htm", FileIconKind::Code},
        {"json", FileIconKind::Code},
        {"rs", FileIconKind::Code},
        {"py", FileIconKind::Code},
        {"c", FileIconKind::Code},
        {"cpp", FileIconKind::Code},
        {"h", FileIconKind::Code},
        {"hpp", FileIconKind::Code},
        {"cs", FileIconKind::Code},
        {"go", FileIconKind::Code},
        {"rb", FileIconKind::Code},
        {"php", FileIconKind::Code},
        {"swift", FileIconKind::Code},
        {"kt", FileIconKind::Code},
        {"kts", FileIconKind::Code},
        {"lua", FileIconKind::Code},
        {"r", FileIconKind::Code},
        {"pl", FileIconKind::Code},
        {"pm", FileIconKind::Code},
        {"java", FileIconKind::Code},
        {"scala", FileIconKind::Code},
        {"clj", FileIconKind::Code},
        {"ex", FileIconKind::Code},
        {"exs", FileIconKind::Code},
        {"erl", FileIconKind::Code},
        {"hs", FileIconKind::Code},
        {"ml", FileIconKind::Code},
        {"fs", FileIconKind::Code},
        {"v", FileIconKind::Code},
        {"sv", FileIconKind::Code},
        {"vhd", FileIconKind::Code},
        {"vhdl", FileIconKind::Code},
        {"zig", FileIconKind::Code},
        {"nim", FileIconKind::Code},
        {"d", FileIconKind::Code},
        {"dart", FileIconKind::Code},
        {"groovy", FileIconKind::Code},
        {"coffee", FileIconKind::Code},
        {"xml", FileIconKind::Markup},
        {"xsl", FileIconKind::Markup},
        {"xslt", FileIconKind::Markup},
        {"xhtml", FileIconKind::Markup},
        {"wxml", FileIconKind::Markup},
        {"xaml", FileIconKind::Markup},
        {"plist", FileIconKind::Markup},
        {"yaml", FileIconKind::Data},
        {"yml", FileIconKind::Data},
        {"toml", FileIconKind::Data},
        {"ini", FileIconKind::Data},
        {"cfg", FileIconKind::Data},
        {"conf", FileIconKind::Data},
        {"properties", FileIconKind::Data},
        {"env", FileIconKind::Data},
        {"sh", FileIconKind::Script},
        {"bash", FileIconKind::Script},
        {"zsh", FileIconKind::Script},
        {"fish", FileIconKind::Script},
        {"bat", FileIconKind::Script},
        {"cmd", FileIconKind::Script},
        {"ps1", FileIconKind::Script},
        {"psm1", FileIconKind::Script},
        {"vbs", FileIconKind::Script},
        {"ahk", FileIconKind::Script},
        {"sql", FileIconKind::Database},
        {"sqlite", FileIconKind::Database},
        {"db", FileIconKind::Database},
        {"mdb", FileIconKind::Database},
        {"accdb", FileIconKind::Database},
        {"ttf", FileIconKind::Font},
        {"otf", FileIconKind::Font},
        {"woff", FileIconKind::Font},
        {"woff2", FileIconKind::Font},
        {"eot", FileIconKind::Font},
        {"zip", FileIconKind::Archive},
        {"tar", FileIconKind::Archive},
        {"gz", FileIconKind::Archive},
        {"bz2", FileIconKind::Archive},
        {"7z", FileIconKind::Archive},
        {"rar", FileIconKind::Archive},
        {"xz", FileIconKind::Archive},
        {"zst", FileIconKind::Archive},
        {"lz", FileIconKind::Archive},
        {"lzma", FileIconKind::Archive},
        {"cab", FileIconKind::Archive},
        {"iso", FileIconKind::Archive},
        {"dmg", FileIconKind::Archive},
        {"img", FileIconKind::Disk},
        {"vhdx", FileIconKind::Disk},
        {"vmdk", FileIconKind::Disk},
        {"qcow2", FileIconKind::Disk},
        {"exe", FileIconKind::Executable},
        {"msi", FileIconKind::Executable},
        {"app", FileIconKind::Executable},
        {"deb", FileIconKind::Executable},
        {"rpm", FileIconKind::Executable},
        {"appimage", FileIconKind::Executable},
        {"snap", FileIconKind::Executable},
        {"flatpak", FileIconKind::Executable},
        {"apk", FileIconKind::Executable},
        {"dll", FileIconKind::Library},
        {"so", FileIconKind::Library},
        {"dylib", FileIconKind::Library},
        {"a", FileIconKind::Library},
        {"lib", FileIconKind::Library},
        {"o", FileIconKind::Library},
        {"obj", FileIconKind::Library},
        {"pem", FileIconKind::Key},
        {"crt", FileIconKind::Key},
        {"cer", FileIconKind::Key},
        {"pub", FileIconKind::Key},
        {"asc", FileIconKind::Key},
        {"gpg", FileIconKind::Key},
        {"pfx", FileIconKind::Key},
        {"p12", FileIconKind::Key},
        {"fbx", FileIconKind::Threed},
        {"stl", FileIconKind::Threed},
        {"blend", FileIconKind::Threed},
        {"3ds", FileIconKind::Threed},
        {"dae", FileIconKind::Threed},
        {"gltf", FileIconKind::Threed},
        {"glb", FileIconKind::Threed},
    };
    return types.value(QFileInfo(name).suffix().toLower(), FileIconKind::File);
}

QIcon fileIcon(const QString &name, const QColor &color)
{
    return cached(color, fileIconKind(name));
}

}
