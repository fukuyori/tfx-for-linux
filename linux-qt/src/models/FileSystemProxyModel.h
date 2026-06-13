#pragma once

#include "models/FileColumns.h"

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
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

    // The proxy exposes more columns (mode/modified/git) than the underlying
    // QFileSystemModel provides, so indices for those "extra" columns must be
    // synthesised explicitly.
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

private:
    int sourceColumnCount() const;

    QHash<QString, QString> m_gitStatuses;
    QString m_fileForeground = "#D9E1E8";
    QString m_directoryForeground = "#E5EDF3";
};
