#pragma once

#include "AppConfig.h"
#include "FilePane.h"
#include "PreviewPane.h"
#include "TerminalPane.h"

#include <QFileSystemModel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QSplitter>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath, QWidget *parent = nullptr);

private:
    void closeEvent(QCloseEvent *event) override;
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
    void setPreviewVisible(bool visible);
    void setTerminalVisible(bool visible);
    void setHiddenFilesVisible(bool visible);
    void setActivePane(FilePane *pane);
    void focusOtherPane();
    void rememberSidebarWidth();
    int fileAreaMinimumWidth(bool splitVisible) const;
    int contentMinimumWidth(bool splitVisible, bool previewVisible) const;
    int visibleFileListWidth() const;
    void syncLayoutConstraints();
    QList<int> normalizedMainSplitterSizes(int sidebarWidth, int fileWidth, int previewWidth) const;
    QList<int> normalizedFileSplitterSizes(int leftWidth, int rightWidth) const;
    void rememberSplitWidth();
    void rememberPreviewWidth();
    FilePane *activePane() const;

    QFileSystemModel *m_treeModel;
    QTreeView *m_treeView;
    QListWidget *m_pinnedList;
    QWidget *m_pinnedSpacer;
    QLineEdit *m_searchEdit;
    QToolBar *m_topToolbar;
    QToolButton *m_splitButton;
    QToolButton *m_previewButton;
    QWidget *m_sidebar;
    QAction *m_splitAction = nullptr;
    QAction *m_previewAction = nullptr;
    QAction *m_terminalAction = nullptr;
    QAction *m_hiddenAction = nullptr;
    FilePane *m_leftPane;
    FilePane *m_rightPane;
    FilePane *m_activePane;
    PreviewPane *m_previewPane;
    TerminalPane *m_terminalPane;
    QSplitter *m_fileSplitter;
    QSplitter *m_mainSplitter;
    QSplitter *m_verticalSplitter;
    AppConfig m_config;
    QString m_initialPath;
    bool m_showHiddenFiles = false;
    bool m_isRestoringSettings = true;
    int m_lastSidebarWidth = 240;
    int m_lastSplitWidth = 420;
    int m_lastPreviewWidth = 360;
};
