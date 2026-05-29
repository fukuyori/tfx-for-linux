#pragma once

#include <QFileSystemModel>
#include <QItemSelection>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStack>
#include <QTabBar>
#include <QTableView>
#include <QWidget>

class FileSystemProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FileSystemProxyModel(QObject *parent = nullptr);
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

class FilePane : public QWidget
{
    Q_OBJECT

public:
    explicit FilePane(const QString &label, const QString &initialPath, QWidget *parent = nullptr);

    QString currentPath() const;
    QList<QUrl> selectedUrls() const;
    void setShowHiddenFiles(bool show);
    void setPathFilter(const QString &text);
    void setActive(bool active);
    QStringList tabPaths() const;
    int activeTabIndex() const;
    void restoreTabs(const QStringList &paths, int activeIndex);
    void navigateTo(const QString &path, bool recordHistory = true);

signals:
    void activated(FilePane *pane);
    void directoryChanged(const QString &path);
    void selectionPreviewRequested(const QString &path);
    void statusMessageRequested(const QString &message);
    void pinFolderRequested(const QString &path);
    void openTerminalHereRequested(const QString &path);

public slots:
    void goUp();
    void goBack();
    void goForward();
    void reload();
    void openSelected();
    void renameSelected();
    void createFolder();
    void createFile();
    void moveSelectedToTrash();
    void copySelected();
    void cutSelected();
    void pasteIntoCurrentDirectory();
    void copySelectedPaths();
    void showColumnSettingsDialog();
    void newTab();
    void closeCurrentTab();
    void nextTab();
    void previousTab();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QModelIndex currentSourceIndex() const;
    QFileInfo currentFileInfo() const;
    QString uniqueChildPath(const QString &baseName) const;
    void showFileContextMenu(const QPoint &point);
    void showEmptyAreaContextMenu(const QPoint &point);
    void showColumnContextMenu(const QPoint &point);
    QString columnTitle(int column) const;
    void resetColumns();
    void revealSelectionInFileManager();
    void openTerminalHere();
    void compressSelectedItemsToZip();
    void extractSelectedZip();
    void selectAllVisibleItems();
    void restoreColumnSettings();
    void saveColumnSettings();
    void updatePreviewFromSelection();
    void updateStatusLine();
    QString displayPath(const QString &path) const;
    QString tabTitleForPath(const QString &path) const;
    void updateCurrentTabPath(const QString &path);
    void pushHistory(const QString &path);
    void setCurrentIndexForPath(const QString &path);

    QFileSystemModel *m_model;
    FileSystemProxyModel *m_proxyModel;
    QTabBar *m_tabBar;
    QTableView *m_view;
    QLabel *m_badgeLabel;
    QLabel *m_pathLabel;
    QLabel *m_statusLabel;
    QString m_label;
    QString m_currentPath;
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;
    bool m_showHiddenFiles = false;
    bool m_isActive = false;
    bool m_isSwitchingTabs = false;
};
