#pragma once

#include "AppConfig.h"
#include "CommandOutputPane.h"
#include "FilePane.h"
#include "PreviewPane.h"
#include "TerminalPane.h"

#include <QFileSystemModel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QDockWidget>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

class QProgressBar;
class QPushButton;
class QThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath,
                        const QString &geometryOverride = QString(),
                        QWidget *parent = nullptr);

private:
    void closeEvent(QCloseEvent *event) override;
    void startFileOperation(const QVector<FileOperationRequest> &requests);
    void cancelFileOperation();
    void completeFileOperation(const QStringList &directories, const QString &message);
    void reloadChangedDirectories(const QStringList &directories);
    void buildActions();
    void buildTopToolbar();
    void buildFolderSidebar(const QString &initialPath);
    void applyTerminalTheme();
    void addPinnedFolder(const QString &path);
    void removePinnedFolder(const QString &path);
    void updatePinnedFolderArea();
    void restoreSettings();
    void saveSettings();
    void setSplitVisible(bool visible);
    void setSidebarVisible(bool visible);
    void setPreviewVisible(bool visible);
    void setTerminalVisible(bool visible);
    void setCommandOutputVisible(bool visible);
    void setHiddenFilesVisible(bool visible);
    void setIconViewEnabled(bool enabled);
    void collapseFolderTree();
    void syncIconViewToggle();
    void setActivePane(FilePane *pane);
    void focusOtherPane();
    QDockWidget *makeDock(const QString &objectName, const QString &title, QWidget *content);
    void applyDefaultDockLayout();
    void resetDockLayout();
    FilePane *activePane() const;

    QFileSystemModel *m_treeModel;
    QTreeView *m_treeView;
    QListWidget *m_pinnedList;
    QWidget *m_pinnedSpacer;
    QLineEdit *m_searchEdit;
    QToolBar *m_topToolbar;
    QToolButton *m_splitButton;
    QToolButton *m_previewButton;
    QToolButton *m_iconViewButton = nullptr;
    QToolButton *m_terminalButton = nullptr;
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
    QProgressBar *m_fileOperationProgress = nullptr;
    QPushButton *m_fileOperationCancel = nullptr;
    QThread *m_fileOperationThread = nullptr;
    FileOperationWorker *m_fileOperationWorker = nullptr;
    QVector<FileOperationRequest> m_queuedFileOperations;
    bool m_closeAfterFileOperationCancel = false;
    QDockWidget *m_dockSidebar = nullptr;
    QDockWidget *m_dockLeftPane = nullptr;
    QDockWidget *m_dockRightPane = nullptr;
    QDockWidget *m_dockPreview = nullptr;
    QDockWidget *m_dockTerminal = nullptr;
    QDockWidget *m_dockCommandOutput = nullptr;
    AppConfig m_config;
    QString m_initialPath;
    QString m_geometryOverride;
    bool m_showHiddenFiles = false;
    bool m_isRestoringSettings = true;
};
