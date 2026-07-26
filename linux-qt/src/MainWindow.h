#pragma once

#include "AppConfig.h"
#include "CommandOutputPane.h"
#include "FilePane.h"
#include "PreviewPane.h"
#include "TerminalPane.h"

#include <QFileSystemModel>
#include <QComboBox>
#include <QListWidget>
#include <QMainWindow>
#include <QDockWidget>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

class QFileSystemWatcher;
class QProgressBar;
class QPushButton;
class QShortcut;
class QSocketNotifier;
class QSplitter;
class QThread;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath,
                        const QString &geometryOverride = QString(),
                        QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupIntegratedTitleBar();
    void startFileOperation(const QVector<FileOperationRequest> &requests);
    void cancelFileOperation();
    void completeFileOperation(const QStringList &directories, const QString &message, bool continueQueued);
    void reloadChangedDirectories(const QStringList &directories);
    int queuedFileOperationCount() const;
    void updateFileOperationSummary(int completed = -1, int total = -1);
    void buildActions();
    void buildTopToolbar();
    void buildFolderSidebar(const QString &initialPath);
    void applyTerminalTheme();
    QString buildThemeStyleSheet() const;
    void applyPaneThemeSettings();
    void addPinnedFolder(const QString &path);
    void removePinnedFolder(const QString &path);
    void updatePinnedFolderArea();
    struct DiskEntry
    {
        QString root;
        QString title;
        QString tooltip;
        double usage = -1.0;
    };
    void refreshDiskList();
    void applyDiskList(const QList<DiskEntry> &entries);
    void updateDiskSelection(const QString &path);
    QString diskRootForPath(const QString &path) const;
    void applySidebarSectionStates();
    void restoreSettings();
    void saveSettings();
    void setupConfigShortcuts();
    void setupConfigWatcher();
    void reloadConfig();
    void showConfigWarnings();
    void openConfigInEditor();
    void showEditorSettingsDialog();
    void runSearchFromToolbar();
    void rememberSearchTerm(const QString &term);
    void setSplitVisible(bool visible);
    void setSidebarVisible(bool visible);
    void setPreviewVisible(bool visible);
    void setTerminalVisible(bool visible);
    void setCommandOutputVisible(bool visible);
    void setHiddenFilesVisible(bool visible);
    void setIconViewEnabled(bool enabled);
    void collapseFolderTree();
    void syncIconViewToggle();
    void syncFolderTree(const QString &path);
    void collapseTreeBranchesOffPath(const QModelIndex &parent, const QString &targetPath);
    void setActivePane(FilePane *pane);
    void focusOtherPane();
    void swapPanes();
    QDockWidget *makeDock(const QString &objectName, const QString &title, QWidget *content);
    void setDockVisiblePreservingSidebar(QDockWidget *dock, bool visible);
    void applyDefaultDockLayout();
    void resetDockLayout();
    FilePane *activePane() const;

    QFileSystemModel *m_treeModel;
    QTreeView *m_treeView;
    // True while a navigation originates from a folder-tree click, so the
    // resulting sync keeps the tree's scroll position instead of jumping.
    bool m_treeNavigationInProgress = false;
    // Path whose tree node should sit at the top of the viewport. Async row
    // insertion (QFileSystemModel loads directories lazily) shifts the
    // scroll position after the fact, so the scroll is re-applied on each
    // relevant directoryLoaded until the target's own listing arrives.
    QString m_pendingTreeScrollPath;
    // Sidebar sections: DISKS volume list and the collapsible headers.
    QListWidget *m_diskList = nullptr;
    QToolButton *m_pinnedHeader = nullptr;
    QToolButton *m_diskHeader = nullptr;
    QToolButton *m_treeHeader = nullptr;
    bool m_pinnedCollapsed = false;
    bool m_disksCollapsed = false;
    bool m_foldersCollapsed = false;
    // Mount-table watch backing the DISKS refresh: /proc/self/mounts signals
    // mount changes via POLLPRI, which QSocketNotifier surfaces as Exception.
    QSocketNotifier *m_mountsNotifier = nullptr;
    QTimer *m_mountsDebounce = nullptr;
    int m_mountsFd = -1;
    // Mount roots of the listed disks, for syscall-free volume matching.
    QStringList m_diskRoots;
    // Discards results of superseded volume scans.
    int m_diskScanGeneration = 0;
    // Integrated title bar ([window] titleBar = "integrated"): the native
    // title bar is hidden and window controls live in the menu bar.
    bool m_integratedTitleBar = false;
    QToolButton *m_maximizeButton = nullptr;
    // Config-bound shortcuts, recreated on live reload.
    QList<QShortcut *> m_configShortcuts;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QTimer *m_configReloadDebounce = nullptr;
    QListWidget *m_pinnedList;
    QWidget *m_pinnedSpacer;
    QComboBox *m_searchEdit;
    QToolBar *m_topToolbar;
    QToolButton *m_splitButton;
    QToolButton *m_previewButton;
    QToolButton *m_iconViewButton = nullptr;
    QToolButton *m_terminalButton = nullptr;
    QToolButton *m_hiddenButton = nullptr;
    QToolButton *m_sidebarButton = nullptr;
    QToolButton *m_commandOutputButton = nullptr;
    QWidget *m_sidebar;
    QAction *m_splitAction = nullptr;
    QAction *m_sidebarAction = nullptr;
    QAction *m_previewAction = nullptr;
    QAction *m_terminalAction = nullptr;
    QAction *m_commandOutputAction = nullptr;
    QAction *m_hiddenAction = nullptr;
    QAction *m_iconViewAction = nullptr;
    FilePane *m_leftPane;
    FilePane *m_rightPane;
    FilePane *m_activePane;
    PreviewPane *m_previewPane;
    TerminalPane *m_terminalPane;
    CommandOutputPane *m_commandOutputPane;
    QLabel *m_fileOperationSummary = nullptr;
    QProgressBar *m_fileOperationProgress = nullptr;
    QPushButton *m_fileOperationCancel = nullptr;
    QThread *m_fileOperationThread = nullptr;
    FileOperationWorker *m_fileOperationWorker = nullptr;
    QVector<FileOperationRequest> m_queuedFileOperations;
    int m_lastFileOperationCompleted = 0;
    int m_lastFileOperationTotal = 0;
    bool m_closeAfterFileOperationCancel = false;
    QDockWidget *m_dockSidebar = nullptr;
    QDockWidget *m_dockFilePanes = nullptr;
    QSplitter *m_paneSplitter = nullptr;
    QDockWidget *m_dockPreview = nullptr;
    QDockWidget *m_dockTerminal = nullptr;
    QDockWidget *m_dockCommandOutput = nullptr;
    AppConfig m_config;
    QString m_initialPath;
    QString m_geometryOverride;
    bool m_showHiddenFiles = false;
    bool m_isRestoringSettings = true;
};
