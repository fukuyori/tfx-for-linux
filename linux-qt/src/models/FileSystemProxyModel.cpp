#include "models/FileSystemProxyModel.h"

#include "UiText.h"
#include "core/FileTypeInfo.h"

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
        switch (section) {
        case ColumnName:
            return UiText::t("NAME", "NAME");
        case ColumnType:
            return UiText::t("TYPE", "TYPE");
        case ColumnSize:
            return UiText::t("SIZE", "SIZE");
        case ColumnCreated:
            return UiText::t("CREATED", "CREATED");
        case ColumnModified:
            return UiText::t("MODIFIED", "MODIFIED");
        case ColumnMode:
            return UiText::t("MODE", "MODE");
        case ColumnGit:
            return QString();
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
        return QColor(info.isDir() ? m_directoryForeground : m_fileForeground);
    }

    if (role == Qt::DecorationRole && index.column() == ColumnName) {
        return fsModel->data(sourceNameIndex, role);
    }

    if (role == Qt::EditRole && index.column() == ColumnName) {
        return fsModel->data(sourceNameIndex, role);
    }

    if (role == Qt::DisplayRole) {
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

Qt::ItemFlags FileSystemProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
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
    switch (sortColumn()) {
    case ColumnSize:
        return leftInfo.size() < rightInfo.size();
    case ColumnCreated:
        return leftInfo.birthTime() < rightInfo.birthTime();
    case ColumnModified:
        return leftInfo.lastModified() < rightInfo.lastModified();
    case ColumnMode:
        return modeString(leftInfo) < modeString(rightInfo);
    case ColumnGit:
        return m_gitStatuses.value(leftInfo.absoluteFilePath()) < m_gitStatuses.value(rightInfo.absoluteFilePath());
    default:
        return QSortFilterProxyModel::lessThan(left, right);
    }
}
