#include "models/FileSystemProxyModel.h"

#include "UiText.h"
#include "core/FileOperations.h"
#include "core/FileTypeInfo.h"
#include "core/SortOptions.h"
#include "views/FileIcons.h"

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemModel>

using namespace tfx::core;

FileSystemProxyModel::FileSystemProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void FileSystemProxyModel::setGitStatuses(const QHash<QString, QString> &statuses)
{
    m_gitStatuses = statuses;
    invalidate();
}

void FileSystemProxyModel::setThemeColors(const QString &fileForeground, const QString &directoryForeground)
{
    m_fileForeground = fileForeground;
    m_directoryForeground = directoryForeground;
    invalidate();
}

void FileSystemProxyModel::setGitColors(const QHash<QString, QString> &labelColors)
{
    m_gitColors = labelColors;
    invalidate();
}

void FileSystemProxyModel::setNaturalSort(bool enabled)
{
    if (m_naturalSort == enabled) {
        return;
    }
    m_naturalSort = enabled;
    if (m_sortColumn == ColumnName) {
        invalidate();
    }
}

int FileSystemProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return kColumnCount;
}

int FileSystemProxyModel::sourceColumnCount() const
{
    return sourceModel() ? sourceModel()->columnCount() : 0;
}

QModelIndex FileSystemProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column >= sourceColumnCount() && column < kColumnCount) {
        // Synthesise an index for an extra column by reusing the internal id of
        // the source-backed column 0 index for the same row.
        const QModelIndex base = QSortFilterProxyModel::index(row, 0, parent);
        if (!base.isValid()) {
            return QModelIndex();
        }
        return createIndex(row, column, base.internalId());
    }
    return QSortFilterProxyModel::index(row, column, parent);
}

QModelIndex FileSystemProxyModel::parent(const QModelIndex &child) const
{
    if (child.isValid() && child.column() >= sourceColumnCount()) {
        const QModelIndex base = createIndex(child.row(), 0, child.internalId());
        return QSortFilterProxyModel::parent(base);
    }
    return QSortFilterProxyModel::parent(child);
}

QModelIndex FileSystemProxyModel::sibling(int row, int column, const QModelIndex &idx) const
{
    return index(row, column, parent(idx));
}

QModelIndex FileSystemProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (proxyIndex.isValid() && proxyIndex.column() >= sourceColumnCount()) {
        // Extra columns have no backing source cell.
        return QModelIndex();
    }
    return QSortFilterProxyModel::mapToSource(proxyIndex);
}

QVariant FileSystemProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        // The stylesheet that themes QHeaderView::section suppresses the
        // native sort arrow, so the marker is part of the title text instead.
        const QString marker = section == m_sortColumn
            ? QString(sortOrder() == Qt::AscendingOrder ? " \u25b2" : " \u25bc")
            : QString();
        switch (section) {
        case ColumnName:
            return UiText::t("NAME", "NAME") + marker;
        case ColumnType:
            return UiText::t("TYPE", "TYPE") + marker;
        case ColumnSize:
            return UiText::t("SIZE", "SIZE") + marker;
        case ColumnCreated:
            return UiText::t("CREATED", "CREATED") + marker;
        case ColumnModified:
            return UiText::t("MODIFIED", "MODIFIED") + marker;
        case ColumnMode:
            return UiText::t("MODE", "MODE") + marker;
        case ColumnGit:
            return marker.trimmed();
        default:
            break;
        }
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant FileSystemProxyModel::data(const QModelIndex &index, int role) const
{
    const auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel || !index.isValid()) {
        return QSortFilterProxyModel::data(index, role);
    }

    const QModelIndex sourceNameIndex = mapToSource(index.sibling(index.row(), ColumnName));
    const QFileInfo info = fsModel->fileInfo(sourceNameIndex);

    if (role == Qt::TextAlignmentRole && index.column() == ColumnGit) {
        return QVariant(Qt::AlignCenter);
    }
    if (role == Qt::TextAlignmentRole && index.column() == ColumnSize) {
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == ColumnGit) {
            const QString label = m_gitStatuses.value(info.absoluteFilePath()).left(1);
            const QString color = m_gitColors.value(label);
            if (!color.isEmpty()) {
                return QColor(color);
            }
        }
        return QColor(info.isDir() ? m_directoryForeground : m_fileForeground);
    }

    if (role == Qt::DecorationRole && index.column() == ColumnName) {
        // Drawn here rather than taken from the desktop icon theme, so the list
        // keeps one look and follows [colors] like the rest of the row.
        return info.isDir() ? tfx::views::folderIcon(QColor(m_directoryForeground))
                            : tfx::views::fileIcon(QColor(m_fileForeground));
    }

    if (role == Qt::EditRole && index.column() == ColumnName) {
        return fsModel->data(sourceNameIndex, role);
    }

    if (role == Qt::DisplayRole) {
        // The parent entry is a navigation control, not a listed file: its
        // type, size and timestamps describe the folder above and only add
        // noise to the row.
        if (index.column() != ColumnName && info.fileName() == "..") {
            return QString();
        }
        switch (index.column()) {
        case ColumnName:
            return fsModel->data(sourceNameIndex, role);
        case ColumnType:
            return englishTypeName(info);
        case ColumnSize:
            return info.isDir() ? QString() : sizeString(info.size());
        case ColumnCreated:
            return info.birthTime().isValid() ? info.birthTime().toString("yyyy-MM-dd HH:mm:ss") : QString();
        case ColumnModified:
            return info.lastModified().toString("yyyy-MM-dd HH:mm:ss");
        case ColumnMode:
            return modeString(info);
        case ColumnGit:
            return m_gitStatuses.value(info.absoluteFilePath());
        default:
            break;
        }
    }
    return {};
}

bool FileSystemProxyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (fsModel && role == Qt::EditRole && index.isValid() && index.column() == ColumnName) {
        const QModelIndex sourceIndex = mapToSource(index.sibling(index.row(), ColumnName));
        const QFileInfo info = fsModel->fileInfo(sourceIndex);
        const QString oldName = info.fileName();
        const QString newName = value.toString();
        if (!newName.isEmpty() && newName != oldName && !newName.contains('/')
            && QString::compare(oldName, newName, Qt::CaseInsensitive) == 0) {
            // QFileSystemModel refuses a case-only rename on case-insensitive
            // filesystems (the target "already exists"); hop via a temp name.
            // The model picks up the change through its directory watcher.
            return tfx::core::renameWithinDirectory(info.absolutePath(), oldName, newName);
        }
    }
    return QSortFilterProxyModel::setData(index, value, role);
}

Qt::ItemFlags FileSystemProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    // ItemIsDragEnabled matters beyond drag-and-drop itself: without it the
    // base view treats a press on a selected row as the start of a rubber
    // band and collapses the multi-selection on the first pixel of movement,
    // instead of waiting for the drag threshold.
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (index.column() == ColumnName) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool FileSystemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    const QFileInfo leftInfo = fsModel->fileInfo(left.sibling(left.row(), 0));
    const QFileInfo rightInfo = fsModel->fileInfo(right.sibling(right.row(), 0));

    // The parent entry is a navigation control rather than a listed file, so
    // it stays on the first row whichever column and direction are active.
    const bool leftIsParent = leftInfo.fileName() == "..";
    const bool rightIsParent = rightInfo.fileName() == "..";
    if (leftIsParent != rightIsParent) {
        // lessThan is asked in sort order, so invert for descending to keep
        // ".." on top rather than letting it fall to the bottom.
        return sortOrder() == Qt::AscendingOrder ? leftIsParent : rightIsParent;
    }

    const int nameOrder = compareNames(leftInfo, rightInfo);
    switch (m_sortColumn) {
    case ColumnSize:
        // Directories have no meaningful size; group them ahead of files so a
        // size sort does not scatter them through the list.
        if (leftInfo.isDir() != rightInfo.isDir()) {
            return leftInfo.isDir();
        }
        if (leftInfo.size() != rightInfo.size()) {
            return leftInfo.size() < rightInfo.size();
        }
        return nameOrder < 0;
    case ColumnCreated:
        if (leftInfo.birthTime() != rightInfo.birthTime()) {
            return leftInfo.birthTime() < rightInfo.birthTime();
        }
        return nameOrder < 0;
    case ColumnModified:
        if (leftInfo.lastModified() != rightInfo.lastModified()) {
            return leftInfo.lastModified() < rightInfo.lastModified();
        }
        return nameOrder < 0;
    case ColumnMode: {
        const int order = QString::compare(modeString(leftInfo), modeString(rightInfo));
        return order != 0 ? order < 0 : nameOrder < 0;
    }
    case ColumnGit: {
        const int order = QString::compare(m_gitStatuses.value(leftInfo.absoluteFilePath()),
                                           m_gitStatuses.value(rightInfo.absoluteFilePath()));
        return order != 0 ? order < 0 : nameOrder < 0;
    }
    case ColumnType: {
        // The Type column is synthesised here, so the base class would compare
        // the unrelated source column that happens to share this index.
        const int order = QString::compare(englishTypeName(leftInfo), englishTypeName(rightInfo),
                                           Qt::CaseInsensitive);
        return order != 0 ? order < 0 : nameOrder < 0;
    }
    case ColumnName:
    default:
        return nameOrder < 0;
    }
}

int FileSystemProxyModel::compareNames(const QFileInfo &left, const QFileInfo &right) const
{
    if (m_naturalSort) {
        return tfx::core::naturalCompare(left.fileName(), right.fileName());
    }
    return QString::compare(left.fileName(), right.fileName(), sortCaseSensitivity());
}

void FileSystemProxyModel::sort(int column, Qt::SortOrder order)
{
    const bool keyChanged = column != m_sortColumn;
    m_sortColumn = column;

    // Modified/Mode/Git are synthesised past the source model's own columns,
    // so they have no source cell to map onto and the base class gives up on
    // sorting entirely (it needs a valid source sort column). Sorting always
    // runs on the name column and lessThan() consults m_sortColumn for the
    // key the user actually picked.
    QSortFilterProxyModel::sort(ColumnName, order);
    if (keyChanged) {
        // The base class short-circuits when its own column and order are
        // unchanged, which is exactly the case when only our key moved.
        invalidate();
    }

    // The sort marker lives in the header title, so every header cell has to
    // be repainted when the sorted column or direction changes.
    emit headerDataChanged(Qt::Horizontal, 0, kColumnCount - 1);
}
