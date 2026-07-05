#include "views/FileViews.h"

#include <QDebug>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QTextStream>

bool selectionDebugEnabled()
{
    return qEnvironmentVariableIsSet("TFX_SELECTION_DEBUG");
}

void selectionDebugLog(const QString &message)
{
    if (!selectionDebugEnabled()) {
        return;
    }

    qDebug().noquote() << message;

    QFile file("/tmp/tfx-selection.log");
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << message << '\n';
    }
}

void logSelectionState(const char *where, QTableView *view)
{
    if (!selectionDebugEnabled() || !view || !view->selectionModel()) {
        return;
    }

    const QModelIndex current = view->currentIndex();
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
    QStringList selected;
    selected.reserve(rows.size());
    for (const QModelIndex &row : rows) {
        selected << QString::number(row.row());
    }
    QStringList selectedIndexes;
    selectedIndexes.reserve(indexes.size());
    for (const QModelIndex &index : indexes) {
        selectedIndexes << QString("(%1,%2)").arg(index.row()).arg(index.column());
    }

    selectionDebugLog(QString("[selection] %1 current=(%2,%3) selectedRows=[%4] selectedIndexes=[%5] hasFocus=%6")
        .arg(where)
        .arg(current.row())
        .arg(current.column())
        .arg(selected.join(','))
        .arg(selectedIndexes.join(','))
        .arg(view->hasFocus() ? "true" : "false"));
}

QItemSelection rowSelection(const QModelIndex &index)
{
    if (!index.isValid()) {
        return {};
    }
    return QItemSelection(index.sibling(index.row(), 0), index.sibling(index.row(), kColumnCount - 1));
}

QModelIndexList selectedRowIndexes(const QItemSelectionModel *selectionModel, int column)
{
    QModelIndexList rows;
    if (!selectionModel || !selectionModel->model()) {
        return rows;
    }
    QSet<QModelIndex> seen;
    const QItemSelection ranges = selectionModel->selection();
    for (const QItemSelectionRange &range : ranges) {
        for (int row = range.top(); row <= range.bottom(); ++row) {
            const QModelIndex index = selectionModel->model()->index(row, column, range.parent());
            if (index.isValid() && !seen.contains(index)) {
                seen.insert(index);
                rows.append(index);
            }
        }
    }
    return rows;
}
