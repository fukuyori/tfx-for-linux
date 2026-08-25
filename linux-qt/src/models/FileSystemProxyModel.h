#pragma once

#include "models/FileColumns.h"

#include <QFileInfo>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QString>

// Wraps QFileSystemModel to present the tfx column set (name/type/size/created/
// modified/mode/git), English type/size formatting, per-row Git status, and
// theme-aware foreground colours.
class FileSystemProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FileSystemProxyModel(QObject *parent = nullptr);
    void setGitStatuses(const QHash<QString, QString> &statuses);
    void setThemeColors(const QString &fileForeground, const QString &directoryForeground);
    // Per-status-letter badge colours (keys are single-letter labels such as
    // "M", "A", "D", "R", "?", "!", "U").
    void setGitColors(const QHash<QString, QString> &labelColors);
    // Numeric-aware name ordering ("file2" before "file10") for the Name column.
    void setNaturalSort(bool enabled);
    bool naturalSort() const { return m_naturalSort; }
    // The column the user sorted by, which is not the column the base class
    // sorts on for the synthesised columns.
    int sortKeyColumn() const { return m_sortColumn; }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    // Refreshes the header sort marker after the base class re-sorts.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // The proxy exposes more columns (mode/modified/git) than the underlying
    // QFileSystemModel provides, so indices for those "extra" columns must be
    // synthesised explicitly.
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

private:
    int sourceColumnCount() const;
    int compareNames(const QFileInfo &left, const QFileInfo &right) const;

    QHash<QString, QString> m_gitStatuses;
    QHash<QString, QString> m_gitColors;
    int m_sortColumn = ColumnName;
    bool m_naturalSort = false;
    QString m_fileForeground = "#D9E1E8";
    QString m_directoryForeground = "#E5EDF3";
};
