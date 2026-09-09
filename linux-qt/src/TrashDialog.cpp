#include "TrashDialog.h"
#include "AppConfig.h"
#include "views/FileIcons.h"
#include "UiText.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>

TrashDialog::TrashDialog(QWidget *parent, bool embedded) : QDialog(parent)
{
    if (embedded) setWindowFlags(Qt::Widget);
    setWindowTitle(UiText::t("Trash", "ゴミ箱"));
    setObjectName("trashDialog");
    resize(850, 480);
    auto *layout = new QVBoxLayout(this);
    if (embedded) {
        auto *title = new QLabel(UiText::t("Trash", "ゴミ箱"), this);
        layout->addWidget(title);
    }
    m_list = new QTreeWidget(this);
    m_list->setObjectName("trashList");
    m_list->setIconSize(QSize(20, 20));
    setFocusProxy(m_list);
    m_list->setHeaderLabels({UiText::t("Name", "名前"), UiText::t("Original Location", "元の場所"),
                            UiText::t("Deleted", "削除日時"), UiText::t("Status", "状態")});
    m_list->setRootIsDecorated(false);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->setSortingEnabled(true);
    m_list->sortByColumn(2, Qt::DescendingOrder);
    layout->addWidget(m_list);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(UiText::t("Close", "閉じる"));
    m_restore = buttons->addButton(UiText::t("Restore to Original Location", "元の場所へ復元"), QDialogButtonBox::ActionRole);
    m_restore->setObjectName("restoreTrashButton");
    m_refresh = buttons->addButton(UiText::t("Refresh", "更新"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &TrashDialog::refresh);
    connect(m_restore, &QPushButton::clicked, this, &TrashDialog::restoreSelected);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &TrashDialog::updateRestoreButton);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, &TrashDialog::showContextMenu);
    refresh();
}

void TrashDialog::setFileListAppearance(const AppColors &colors, const QFont &font)
{
    m_fileColor = QColor(colors.foreground);
    m_folderColor = QColor(colors.directoryForeground);
    updateItemIcons();
    m_list->setFont(font);
    // Explicit selected rules keep the highlight when focus moves to a button,
    // and prevent the hover colour from hiding selected rows.
    m_list->setStyleSheet(QStringLiteral(
        "QTreeWidget#trashList { background: %1; color: %2; border: 0; outline: 0;"
        " selection-background-color: %3; selection-color: %4;"
        " font-family: '%7'; font-size: %8px; }"
        "QTreeWidget#trashList > QWidget { background: transparent; }"
        "QTreeWidget#trashList::item { padding: 0px 6px; margin: 0; border-radius: 0; }"
        "QTreeWidget#trashList::item:hover:!selected { background: %5; }"
        "QTreeWidget#trashList::item:selected,"
        "QTreeWidget#trashList::item:selected:active,"
        "QTreeWidget#trashList::item:selected:!active { background: %3; color: %4; }"
        "QTreeWidget#trashList QHeaderView::section { background: %1; color: %6; }")
        .arg(colors.panelBackground, colors.foreground, colors.selectedBackground,
             colors.selectedForeground, colors.hoverBackground, colors.headerForeground,
             QString(font.family()).replace('\\', "\\\\").replace('\'', "\\'"))
        .arg(font.pixelSize() > 0 ? font.pixelSize() : 13));
}

void TrashDialog::updateItemIcons()
{
    const QIcon folder = tfx::views::folderIcon(m_folderColor);
    for (int row = 0; row < m_list->topLevelItemCount(); ++row) {
        auto *item = m_list->topLevelItem(row);
        const auto &entry = m_entries.at(item->data(0, Qt::UserRole).toInt());
        item->setIcon(0, entry.isDirectory ? folder
            : tfx::views::fileIcon(entry.originalPath.isEmpty() ? entry.id : entry.originalPath, m_fileColor));
    }
}

void TrashDialog::showContextMenu(const QPoint &point)
{
    if (m_busy) return;
    auto *item = m_list->itemAt(point);
    if (!item) return;
    if (!item->isSelected()) {
        m_list->clearSelection();
        m_list->setCurrentItem(item);
        item->setSelected(true);
    }
    QMenu menu(this);
    auto *restore = menu.addAction(UiText::t("Restore to Original Location", "元の場所へ復元"),
                                  this, &TrashDialog::restoreSelected);
    restore->setObjectName("restoreTrashAction");
    restore->setEnabled(m_restore->isEnabled());
    auto *remove = menu.addAction(UiText::t("Delete Permanently", "完全に削除"),
                                 this, &TrashDialog::deleteSelected);
    remove->setObjectName("deleteTrashAction");
    menu.exec(m_list->viewport()->mapToGlobal(point));
}

void TrashDialog::deleteSelected()
{
    if (m_busy) return;
    QVector<tfx::core::TrashEntry> selected;
    for (auto *item : m_list->selectedItems())
        selected.append(m_entries.at(item->data(0, Qt::UserRole).toInt()));
    if (selected.isEmpty()) return;
    if (QMessageBox::warning(this, UiText::t("Delete Permanently", "完全に削除"),
        UiText::t("Permanently delete %1 item(s)? This cannot be undone.",
                  "%1 件を完全に削除しますか？この操作は元に戻せません。").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;

    m_busy = true;
    m_list->setEnabled(false);
    m_refresh->setEnabled(false);
    updateRestoreButton();
    m_status->setText(UiText::t("Deleting…", "削除中…"));
    auto errors = std::make_shared<QStringList>();
    auto *thread = QThread::create([selected, errors]() {
        for (const auto &entry : selected) {
            QString error;
            tfx::core::deleteTrashEntry(entry, &error);
            if (!error.isEmpty()) errors->append(error);
        }
    });
    connect(thread, &QThread::finished, this, [this, errors]() {
        m_busy = false;
        m_list->setEnabled(true);
        if (!errors->isEmpty()) {
            QMessageBox message(QMessageBox::Warning, windowTitle(),
                UiText::t("Some items could not be completely deleted. Check the details.",
                          "完全削除または削除情報の後処理に失敗した項目があります。詳細を確認してください。"),
                QMessageBox::Ok, this);
            message.setDetailedText(errors->join('\n'));
            message.exec();
        }
        refresh();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void TrashDialog::updateRestoreButton()
{
    bool restorable = false;
    for (auto *item : m_list->selectedItems()) {
        if (m_entries.at(item->data(0, Qt::UserRole).toInt()).error.isEmpty()) restorable = true;
    }
    m_restore->setEnabled(!m_busy && restorable);
}

void TrashDialog::refresh()
{
    if (m_busy) return;
    m_busy = true;
    m_refresh->setEnabled(false);
    updateRestoreButton();
    m_status->setText(UiText::t("Loading trash…", "ゴミ箱を読み込み中…"));
    auto result = std::make_shared<tfx::core::TrashListing>();
    auto *thread = QThread::create([result]() {
        *result = tfx::core::listTrash(tfx::core::trashLocations());
    });
    connect(thread, &QThread::finished, this, [this, result]() {
        m_list->clear();
        m_entries = result->entries;
        m_list->setSortingEnabled(false);
        for (int i = 0; i < m_entries.size(); ++i) {
            const auto &entry = m_entries.at(i);
            auto *item = new QTreeWidgetItem(m_list, {
                entry.originalPath.isEmpty() ? entry.id : QFileInfo(entry.originalPath).fileName(),
                entry.originalPath,
                entry.deletionDate.toString("yyyy-MM-dd HH:mm:ss"),
                entry.error.isEmpty() ? QString() : UiText::t("Cannot restore", "復元不可")});
            item->setData(0, Qt::UserRole, i);
            item->setToolTip(1, entry.originalPath);
            item->setToolTip(3, entry.error);
        }
        updateItemIcons();
        m_list->setSortingEnabled(true);
        m_list->resizeColumnToContents(0);
        m_list->resizeColumnToContents(2);
        m_status->setText((m_entries.isEmpty() && result->errors.isEmpty()
            ? UiText::t("Trash is empty.", "ゴミ箱は空です。")
            : UiText::t("%1 item(s)", "%1 件").arg(m_entries.size()))
            + (result->errors.isEmpty() ? QString() : "\n" + result->errors.join('\n')));
        m_busy = false;
        m_refresh->setEnabled(true);
        updateRestoreButton();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void TrashDialog::restoreSelected()
{
    if (m_busy) return;
    QVector<tfx::core::TrashEntry> selected;
    for (auto *item : m_list->selectedItems()) {
        const auto &entry = m_entries.at(item->data(0, Qt::UserRole).toInt());
        if (entry.error.isEmpty()) selected.append(entry);
    }
    if (selected.isEmpty()) return;
    if (QMessageBox::question(this, windowTitle(),
        UiText::t("Restore %1 item(s) to their original locations? Existing files will not be overwritten.",
                  "%1 件を元の場所へ復元しますか？既存のファイルは上書きしません。").arg(selected.size())) != QMessageBox::Yes) return;
    QStringList directories;
    QStringList errors;
    // Only atomic filesystem renames are performed; no recursive data copies.
    for (const auto &entry : selected) {
        QString error;
        if (tfx::core::restoreTrashEntry(entry, &error))
            directories.append(QFileInfo(entry.originalPath).absolutePath());
        if (!error.isEmpty()) errors.append(error);
    }
    if (!directories.isEmpty()) emit restored(directories);
    if (!errors.isEmpty()) {
        QMessageBox message(QMessageBox::Warning, windowTitle(),
            UiText::t("Some items could not be restored or cleaned up. Check the details.",
                      "復元または復元情報の後処理に失敗した項目があります。詳細を確認してください。"),
            QMessageBox::Ok, this);
        message.setDetailedText(errors.join('\n'));
        message.exec();
    }
    refresh();
}
