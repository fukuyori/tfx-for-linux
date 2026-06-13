#pragma once

class QFileSystemWatcher;
class QTimer;
class QStackedWidget;
class QDirIterator;
class QStandardItemModel;
class QListView;
class GitStatusController;

#include "models/FileSystemProxyModel.h"

#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QHash>
#include <QItemSelection>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QStack>
#include <QTabBar>
#include <QTableView>
#include <QWidget>

class FilePane : public QWidget
{
    Q_OBJECT

public:
    explicit FilePane(const QString &label, const QString &initialPath, QWidget *parent = nullptr);
    ~FilePane() override;

    QString currentPath() const;
    QList<QUrl> selectedUrls() const;
    void setShowHiddenFiles(bool show);
    void setPathFilter(const QString &text);
    void startSearch(const QString &term);
    void setViewMode(bool iconMode);
    bool isIconMode() const { return m_iconMode; }
    void setActive(bool active);
    void setThemeColors(const QString &fileForeground, const QString &directoryForeground);
    QStringList tabPaths() const;
    int activeTabIndex() const;
    void restoreTabs(const QStringList &paths, int activeIndex);
    void navigateTo(const QString &path, bool recordHistory = true);
    void focusFileList();
    void applySharedColumnLayout();

signals:
    void activated(FilePane *pane);
    void directoryChanged(const QString &path);
    void selectionPreviewRequested(const QString &path);
    void multiSelectionPreviewRequested(const QStringList &paths);
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
    void applyDefaultColumns();
    void saveColumnSettings();
    void searchStep();
    void cancelSearch();
    void refreshGitStatuses();
    void createLinkForSelection();
    void openWithCustomApplication();
    void performDrop(const QList<QUrl> &urls, Qt::DropAction action, const QString &targetDir);
    void updatePreviewFromSelection();
    void updateStatusLine();
    void commitPathEditor();
    QString displayPath(const QString &path) const;
    QString tabTitleForPath(const QString &path) const;
    void updateCurrentTabPath(const QString &path);
    void pushHistory(const QString &path);
    void selectProxyIndex(const QModelIndex &index);
    bool selectParentEntry();
    void setCurrentIndexForPath(const QString &path);

    QFileSystemModel *m_model;
    FileSystemProxyModel *m_proxyModel;
    QTabBar *m_tabBar;
    QTableView *m_view;
    QLabel *m_badgeLabel;
    QLineEdit *m_pathEdit;
    QLabel *m_statusLabel;
    GitStatusController *m_gitController = nullptr;
    QString m_gitBranch;
    QFileSystemWatcher *m_dirWatcher = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    QListView *m_iconView = nullptr;
    bool m_iconMode = false;
    QTableView *m_searchView = nullptr;
    QStandardItemModel *m_searchModel = nullptr;
    QFileIconProvider m_iconProvider;
    QDirIterator *m_searchIterator = nullptr;
    QTimer *m_searchTimer = nullptr;
    QString m_searchTerm;
    int m_searchMatches = 0;
    QString m_fileForeground = "#D9E1E8";
    QString m_directoryForeground = "#E5EDF3";
    QString m_label;
    QString m_currentPath;
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;
    bool m_showHiddenFiles = false;
    bool m_isActive = false;
    bool m_suppressColumnSave = false;
    bool m_isSwitchingTabs = false;
};
