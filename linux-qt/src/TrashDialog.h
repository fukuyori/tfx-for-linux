#pragma once

#include "core/Trash.h"
#include <QDialog>
#include <QColor>

class QLabel;
class QPushButton;
class QTreeWidget;
struct AppColors;

class TrashDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TrashDialog(QWidget *parent = nullptr, bool embedded = false);
    void setFileListAppearance(const AppColors &colors, const QFont &font);
    void refresh();

signals:
    void restored(const QStringList &directories);

private:
    void restoreSelected();
    void deleteSelected();
    void showContextMenu(const QPoint &point);
    void updateRestoreButton();
    void updateItemIcons();
    QTreeWidget *m_list;
    QLabel *m_status;
    QPushButton *m_restore;
    QPushButton *m_refresh;
    QVector<tfx::core::TrashEntry> m_entries;
    bool m_busy = false;
    QColor m_fileColor{QStringLiteral("#F2F2F2")};
    QColor m_folderColor{QStringLiteral("#FFFFFF")};
};
