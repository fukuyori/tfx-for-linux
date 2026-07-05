#pragma once

class QFileSystemWatcher;
class QTimer;
class QStackedWidget;
class QDirIterator;
class QStandardItemModel;
class QListView;
class GitStatusController;
class QMenu;
class QHBoxLayout;

#include "AppConfig.h"
#include "core/FileOperationWorker.h"
#include "models/FileSystemProxyModel.h"

#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QFont>
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
    void setGitStatusColors(const QHash<QString, QString> &labelColors);
    void setDropTargetColor(const QString &color);
    QStringList tabPaths() const;
    int activeTabIndex() const;
    void restoreTabs(const QStringList &paths, int activeIndex);
    void navigateTo(const QString &path, bool recordHistory = true);
    void focusFileList();
    void applySharedColumnLayout();
    void setUserCommands(const QList<UserCommand> &commands);
    void setOpenWithApplications(const QHash<QString, QString> &applications);
    void setPlaceholderLanguage(const QString &language);
    void runUserCommand(int index);

signals:
    void activated(FilePane *pane);
    void directoryChanged(const QString &path);
    void selectionPreviewRequested(const QString &path);
    void multiSelectionPreviewRequested(const QStringList &paths);
    void statusMessageRequested(const QString &message);
    void pinFolderRequested(const QString &path);
    void openTerminalHereRequested(const QString &path);
    void tabsChanged();
    void fileOperationPathsChanged(const QStringList &directories);
    void fileOperationRequested(const QVector<FileOperationRequest> &requests);
    void commandOutputReady(const QString &name,
                            const QString &commandLine,
                            const QString &workingDirectory,
                            int exitCode,
                            QProcess::ExitStatus exitStatus,
                            const QString &stdoutText,
                            const QString &stderrText,
                            bool reveal);
    // Streaming counterparts for commands with terminal = true: output flows to
    // the terminal pane's Output tab as it arrives.
    void terminalCommandStarted(const QString &header);
    void terminalCommandOutput(const QString &chunk);
    void terminalCommandFinished(const QString &footer);

public slots:
    void goUp();
    void goBack();
    void goForward();
    void reload();
    void cancelSearch();
    void openSelected();
    void renameSelected();
    void createFolder();
    void createFile();
    void moveSelectedToTrash();
    void copySelected();
    void cutSelected();
    void pasteIntoCurrentDirectory();
    void movePasteIntoCurrentDirectory();
    void pasteClipboardAsPlainText();
    void copySelectedPaths();
    void showColumnSettingsDialog();
    void newTab();
    void closeCurrentTab();
    void nextTab();
    void previousTab();
    void revealSelectionInFileManager();
    void openTerminalHere();
    void selectAllVisibleItems();
    void compressSelectedItemsToZip();
    void extractSelectedZip();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // UI setup
    void setupPaneChrome(const QString &initialPath);
    QWidget *createHeaderLayout();
    void setupFileView();
    void setupSearchView();
    void setupIconView();
    void setupZipView();
    void setupViewStack();

    // Signal wiring and refresh
    void setupGitRefresh();
    void setupSearchConnections();
    void setupFileViewConnections();
    void setupTabConnections();

    // State and selection
    QModelIndex currentSourceIndex() const;
    QFileInfo currentFileInfo() const;
    QStringList selectedLocalPaths() const;
    void refreshGitStatuses();
    void updatePreviewFromSelection();
    void updatePreviewFromSearchSelection();
    void updateStatusLine();

    // Navigation
    void commitPathEditor();
    QString displayPath(const QString &path) const;
    void pushHistory(const QString &path);
    void selectProxyIndex(const QModelIndex &index);
    bool selectParentEntry();
    void setCurrentIndexForPath(const QString &path);

    // Search
    void searchStep();
    QString searchResultPath(const QModelIndex &index) const;
    QStringList selectedSearchResultPaths() const;
    void showSearchContextMenu(const QPoint &point);

    // Tabs
    void showTabContextMenu(const QPoint &point);
    QString tabTitleForPath(const QString &path) const;
    int tabIndexForPath(const QString &path) const;
    void updateCurrentTabPath(const QString &path);
    void updateTabCloseButtons();

    // File actions and operations
    QString uniqueChildPath(const QString &baseName) const;
    QString placeholderText(const QString &english, const QString &japanese) const;
    void showFileContextMenu(const QPoint &point);
    void showEmptyAreaContextMenu(const QPoint &point);
    void createLinkForSelection();
    void openWithConfiguredApplication(const QString &program);
    void openWithCustomApplication();
    bool tryRunExecutable(const QFileInfo &info);
    bool launchExecutable(const QFileInfo &info);

    // Clipboard and drag/drop
    bool pasteClipboardAsFile(bool plainTextOnly);
    void performDrop(const QList<QUrl> &urls, Qt::DropAction action, const QString &targetDir);
    void pasteClipboard(bool forceMove);

    // Archives
    void openZip(const QString &path);
    void populateZipView();
    void exitZipMode();

    // Columns
    void showColumnContextMenu(const QPoint &point);
    QString columnTitle(int column) const;
    void resetColumns();
    void applyDefaultColumns();
    void saveColumnSettings();

    // User commands
    void addUserCommandActions(QMenu *menu, bool hasSelection);

    QFileSystemModel *m_model;
    FileSystemProxyModel *m_proxyModel;

    // Core widgets
    QTabBar *m_tabBar;
    QTableView *m_view;
    QLabel *m_badgeLabel;
    QLineEdit *m_pathEdit;
    QLabel *m_statusLabel;
    QWidget *m_titleBar = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    QListView *m_iconView = nullptr;
    bool m_iconMode = false;

    // Search view. Directories are walked one non-recursive iterator at a
    // time with a pending queue so the descent depth can be bounded.
    QTableView *m_searchView = nullptr;
    QStandardItemModel *m_searchModel = nullptr;
    QDirIterator *m_searchIterator = nullptr;
    QTimer *m_searchTimer = nullptr;
    QString m_searchTerm;
    int m_searchMatches = 0;
    QVector<QPair<QString, int>> m_searchPendingDirs;
    int m_searchIteratorDepth = 0;
    bool m_searchTruncated = false;
    QDir::Filters m_searchFilters;
    // Session-lifetime icon cache keyed by lowercase extension, so streaming
    // thousands of search results does one mime/icon lookup per extension
    // instead of one per row.
    QHash<QString, QIcon> m_iconCacheByExtension;
    QIcon m_cachedFolderIcon;
    QIcon cachedFileIcon(const QFileInfo &info);

    // Archive view
    QTableView *m_zipView = nullptr;
    QStandardItemModel *m_zipModel = nullptr;
    QFileIconProvider m_iconProvider;
    QString m_zipPath;
    QString m_zipDir;
    QStringList m_zipEntries;
    QStringList m_zipSymlinkEntries;

    // Status and appearance
    GitStatusController *m_gitController = nullptr;
    QString m_gitBranch;
    QFileSystemWatcher *m_dirWatcher = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    QString m_fileForeground = "#D9E1E8";
    QString m_directoryForeground = "#E5EDF3";
    QString m_label;
    QString m_currentPath;
    bool m_showHiddenFiles = false;
    bool m_isActive = false;
    bool m_suppressColumnSave = false;
    bool m_isSwitchingTabs = false;

    // Navigation and configuration
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;
    QList<UserCommand> m_userCommands;
    QHash<QString, QString> m_openWithApplications;
    QString m_placeholderLanguage = "auto";
};
