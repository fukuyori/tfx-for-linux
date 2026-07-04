#include "FilePane.h"
#include "FilePaneColumns.h"
#include "UiText.h"
#include "models/ColumnLayout.h"
#include "models/FileColumns.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>

using namespace tfx::filepane;
using namespace tfx::models;

void FilePane::showColumnContextMenu(const QPoint &point)
{
    QMenu menu(this);
    for (int column = 0; column < kColumnCount; ++column) {
        QAction *action = menu.addAction(columnTitle(column));
        action->setCheckable(true);
        action->setChecked(!m_view->isColumnHidden(column));
        action->setEnabled(column != 0);
        connect(action, &QAction::toggled, this, [this, column](bool visible) {
            m_view->setColumnHidden(column, !visible);
            saveColumnSettings();
        });
    }

    menu.addSeparator();
    menu.addAction(UiText::t("Column Settings...", "表示項目設定..."), this, &FilePane::showColumnSettingsDialog);
    menu.addAction(UiText::t("Reset Columns", "カラムをリセット"), this, &FilePane::resetColumns);

    menu.exec(m_view->horizontalHeader()->mapToGlobal(point));
}

QString FilePane::columnTitle(int column) const
{
    switch (column) {
    case ColumnName:
        return UiText::t("Name", "名前");
    case ColumnType:
        return UiText::t("Type", "種類");
    case ColumnSize:
        return UiText::t("Size", "サイズ");
    case ColumnCreated:
        return UiText::t("Date Created", "作成日時");
    case ColumnModified:
        return UiText::t("Date Modified", "更新日時");
    case ColumnMode:
        return UiText::t("File Mode", "ファイルモード");
    case ColumnGit:
        return UiText::t("Git Status", "Git ステータス");
    default:
        return {};
    }
}

void FilePane::resetColumns()
{
    const QSignalBlocker headerBlocker(m_view->horizontalHeader());
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        m_view->horizontalHeader()->moveSection(m_view->horizontalHeader()->visualIndex(column), column);
        m_view->horizontalHeader()->resizeSection(column, defaultColumnWidth(column));
    }
    saveColumnSettings();
}

void FilePane::showColumnSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(UiText::t("File List Settings", "ファイル一覧設定"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(UiText::t("Columns", "表示項目"), &dialog);
    title->setObjectName("sectionLabel");
    layout->addWidget(title);

    auto *list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    const auto addColumnItem = [this, list](int logical, bool visible) {
        auto *item = new QListWidgetItem(columnTitle(logical), list);
        item->setData(Qt::UserRole, logical);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        if (logical == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
    };
    const auto populateFromHeader = [this, list, addColumnItem]() {
        list->clear();
        for (int visual = 0; visual < kColumnCount; ++visual) {
            const int logical = m_view->horizontalHeader()->logicalIndex(visual);
            addColumnItem(logical, !m_view->isColumnHidden(logical));
        }
    };
    const auto populateDefaults = [list, addColumnItem]() {
        list->clear();
        for (int logical = 0; logical < kColumnCount; ++logical) {
            addColumnItem(logical, true);
        }
    };
    populateFromHeader();
    layout->addWidget(list);

    auto *moveLayout = new QHBoxLayout();
    auto *upButton = new QPushButton(UiText::t("Up", "上へ"), &dialog);
    auto *downButton = new QPushButton(UiText::t("Down", "下へ"), &dialog);
    auto *resetButton = new QPushButton(UiText::t("Reset", "リセット"), &dialog);
    moveLayout->addWidget(upButton);
    moveLayout->addWidget(downButton);
    moveLayout->addStretch(1);
    moveLayout->addWidget(resetButton);
    layout->addLayout(moveLayout);

    connect(upButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row > 0) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row - 1, item);
            list->setCurrentRow(row - 1);
        }
    });
    connect(downButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row >= 0 && row < list->count() - 1) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row + 1, item);
            list->setCurrentRow(row + 1);
        }
    });
    connect(resetButton, &QPushButton::clicked, &dialog, [populateDefaults]() {
        populateDefaults();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    for (int visual = 0; visual < list->count(); ++visual) {
        const int logical = list->item(visual)->data(Qt::UserRole).toInt();
        const int currentVisual = m_view->horizontalHeader()->visualIndex(logical);
        if (currentVisual != visual) {
            m_view->horizontalHeader()->moveSection(currentVisual, visual);
        }
        m_view->setColumnHidden(logical, logical == 0 ? false : list->item(visual)->checkState() != Qt::Checked);
    }
    saveColumnSettings();
}

void FilePane::applyDefaultColumns()
{
    auto *header = m_view->horizontalHeader();
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        header->moveSection(header->visualIndex(column), column);
        header->resizeSection(column, defaultColumnWidth(column));
    }
}

void FilePane::applySharedColumnLayout()
{
    // Column layout (order/visibility/width) is shared between panes: only the
    // left pane persists it and the right pane mirrors the left, so a single
    // (non per-pane) settings group is used. "V2" retires older layouts.
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");
    const bool hasSaved = settings.contains("width0");

    auto *header = m_view->horizontalHeader();
    const QSignalBlocker headerBlocker(header);

    if (!hasSaved) {
        settings.endGroup();
        applyDefaultColumns();
        return;
    }

    const QStringList order = normalizedColumnOrder(settings.value("order").toStringList());
    for (int visual = 0; visual < kColumnCount; ++visual) {
        const int logical = order.at(visual).toInt();
        const int current = header->visualIndex(logical);
        if (current >= 0 && current != visual) {
            header->moveSection(current, visual);
        }
    }

    for (int column = 0; column < kColumnCount; ++column) {
        const bool visible = settings.value(QString("visible%1").arg(column), true).toBool();
        m_view->setColumnHidden(column, column == ColumnName ? false : !visible);
        const int width = settings.value(QString("width%1").arg(column), defaultColumnWidth(column)).toInt();
        header->resizeSection(column, normalizedColumnWidth(width, defaultColumnWidth(column)));
    }
    const int sortColumn = normalizedSortColumn(settings.value("sortColumn", ColumnName).toInt());
    const Qt::SortOrder sortOrder = normalizedSortOrder(
        settings.value("sortOrder", static_cast<int>(Qt::AscendingOrder)).toInt());
    settings.endGroup();

    m_view->sortByColumn(sortColumn, sortOrder);
}

void FilePane::saveColumnSettings()
{
    // Ignore section changes triggered by the model reorganising itself rather
    // than by the user (those would persist bogus reset widths).
    if (m_suppressColumnSave) {
        return;
    }
    // Only the left pane owns the shared column layout.
    if (m_label != "LEFT") {
        return;
    }
    auto *header = m_view->horizontalHeader();
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");

    QStringList order;
    order.reserve(kColumnCount);
    for (int visual = 0; visual < kColumnCount; ++visual) {
        order.append(QString::number(header->logicalIndex(visual)));
    }
    settings.setValue("order", order);

    for (int column = 0; column < kColumnCount; ++column) {
        settings.setValue(QString("visible%1").arg(column), !m_view->isColumnHidden(column));
        settings.setValue(QString("width%1").arg(column), header->sectionSize(column));
    }
    settings.setValue("sortColumn", header->sortIndicatorSection());
    settings.setValue("sortOrder", static_cast<int>(header->sortIndicatorOrder()));
    settings.endGroup();
}
