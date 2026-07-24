#include "MainWindow.h"
#include "MainWindowSidebar.h"
#include "UiText.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFileSystemModel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &initialPath, const QString &geometryOverride, QWidget *parent)
    : QMainWindow(parent),
      m_treeModel(new QFileSystemModel(this)),
      m_treeView(new QTreeView(this)),
      m_pinnedList(new PinnedListWidget(this)),
      m_pinnedSpacer(new QWidget(this)),
      m_searchEdit(new QComboBox(this)),
      m_topToolbar(new QToolBar(this)),
      m_splitButton(new QToolButton(this)),
      m_previewButton(new QToolButton(this)),
      m_sidebar(new QWidget(this)),
      m_leftPane(new FilePane("LEFT", initialPath, this)),
      m_rightPane(new FilePane("RIGHT", QDir::homePath(), this)),
      m_activePane(m_leftPane),
      m_previewPane(new PreviewPane(this)),
      m_terminalPane(new TerminalPane(this)),
      m_commandOutputPane(new CommandOutputPane(this)),
      m_config(AppConfig::loadOrCreate()),
      m_initialPath(initialPath),
      m_geometryOverride(geometryOverride)
{
    setWindowTitle("tfx");
    resize(1280, 780);
    // Translucency must be set before the native surface is created.
    if (m_config.opacity.background < 1.0) {
        setAttribute(Qt::WA_TranslucentBackground);
    }
    m_leftPane->setUserCommands(m_config.commands);
    m_rightPane->setUserCommands(m_config.commands);
    m_leftPane->setOpenWithApplications(m_config.openWith);
    m_rightPane->setOpenWithApplications(m_config.openWith);
    m_leftPane->setPlaceholderLanguage(m_config.naming.placeholderLanguage);
    m_rightPane->setPlaceholderLanguage(m_config.naming.placeholderLanguage);
    applyTerminalTheme();
    buildTopToolbar();
    buildFolderSidebar(initialPath);

    m_sidebar->setMinimumWidth(160);
    m_treeView->setMinimumWidth(140);
    m_pinnedList->setMinimumWidth(140);

    m_sidebar->setObjectName("sidebar");
    auto *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(10, 8, 8, 0);
    sidebarLayout->setSpacing(6);
    auto *pinnedLabel = new QLabel(UiText::t("Pinned", "ピン留め"), m_sidebar);
    auto *treeLabel = new QLabel(UiText::t("Folders", "フォルダー"), m_sidebar);
    auto *collapseTreeButton = new QToolButton(m_sidebar);
    pinnedLabel->setObjectName("sectionLabel");
    treeLabel->setObjectName("sectionLabel");
    collapseTreeButton->setObjectName("toolbarIconButton");
    collapseTreeButton->setText("-");
    collapseTreeButton->setToolTip(UiText::t("Collapse all folders", "すべてのフォルダーを折りたたむ"));
    collapseTreeButton->setFixedSize(24, 22);
    connect(collapseTreeButton, &QToolButton::clicked, this, &MainWindow::collapseFolderTree);
    auto *treeHeaderLayout = new QHBoxLayout();
    treeHeaderLayout->setContentsMargins(0, 0, 0, 0);
    treeHeaderLayout->addWidget(treeLabel);
    treeHeaderLayout->addStretch(1);
    treeHeaderLayout->addWidget(collapseTreeButton);
    sidebarLayout->addWidget(pinnedLabel);
    sidebarLayout->addWidget(m_pinnedList);
    sidebarLayout->addWidget(m_pinnedSpacer);
    sidebarLayout->addLayout(treeHeaderLayout);
    sidebarLayout->addWidget(m_treeView, 1);

    // Each pane lives in a dock widget so it can be rearranged, floated, or
    // hidden. Toggling visibility redistributes space within the window instead
    // of resizing the window. A zero-size central widget lets docks fill it.
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks
                   | QMainWindow::AllowTabbedDocks);
    auto *centerFiller = new QWidget(this);
    centerFiller->setMaximumSize(0, 0);
    setCentralWidget(centerFiller);

    m_dockSidebar = makeDock("dockSidebar", UiText::t("Folders", "フォルダー"), m_sidebar);
    // Both file panes share one dock through a splitter so they always stay
    // adjacent and float as a single unit; split view just hides the right
    // pane inside the splitter.
    m_paneSplitter = new QSplitter(Qt::Horizontal, this);
    m_paneSplitter->setObjectName("filePaneSplitter");
    m_paneSplitter->setChildrenCollapsible(false);
    m_paneSplitter->addWidget(m_leftPane);
    m_paneSplitter->addWidget(m_rightPane);
    m_dockFilePanes = makeDock("dockFilePanes", QString(), m_paneSplitter);
    m_dockPreview = makeDock("dockPreview", UiText::t("Preview", "プレビュー"), m_previewPane);
    m_dockTerminal = makeDock("dockTerminal", UiText::t("Terminal", "ターミナル"), m_terminalPane);
    m_dockCommandOutput = makeDock("dockCommandOutput", UiText::t("Command Output", "コマンド出力"), m_commandOutputPane);

    applyDefaultDockLayout();
    m_dockTerminal->hide();
    m_dockCommandOutput->hide();

    const auto wirePane = [this](FilePane *pane) {
        connect(pane, &FilePane::activated, this, [this](FilePane *activatedPane) {
            setActivePane(activatedPane);
            m_terminalPane->setWorkingDirectory(activatedPane->currentPath());
        });
        connect(pane, &FilePane::directoryChanged, this, [this, pane](const QString &path) {
            if (pane == m_activePane) {
                m_treeView->setCurrentIndex(m_treeModel->index(path));
                m_terminalPane->setWorkingDirectory(path);
            }
            if (!m_isRestoringSettings) {
                saveSettings();
            }
        });
        connect(pane, &FilePane::selectionPreviewRequested, m_previewPane, &PreviewPane::previewPath);
        connect(pane, &FilePane::multiSelectionPreviewRequested, m_previewPane, &PreviewPane::previewSelection);
        connect(pane, &FilePane::statusMessageRequested, this, [this](const QString &message) {
            statusBar()->showMessage(message, 3500);
        });
        connect(pane, &FilePane::pinFolderRequested, this, &MainWindow::addPinnedFolder);
        connect(pane, &FilePane::openTerminalHereRequested, this, [this](const QString &path) {
            m_terminalPane->openAt(path);
            setTerminalVisible(true);
        });
        connect(pane, &FilePane::tabsChanged, this, [this]() {
            if (!m_isRestoringSettings) {
                saveSettings();
            }
        });
        connect(pane, &FilePane::fileOperationPathsChanged, this, [this](const QStringList &directories) {
            reloadChangedDirectories(directories);
        });
        connect(pane, &FilePane::fileOperationRequested, this, &MainWindow::startFileOperation);
        connect(pane, &FilePane::commandOutputReady, this,
                [this](const QString &name,
                       const QString &commandLine,
                       const QString &workingDirectory,
                       int exitCode,
                       QProcess::ExitStatus exitStatus,
                       const QString &stdoutText,
                       const QString &stderrText,
                       bool reveal) {
            m_commandOutputPane->appendOutput(name, commandLine, workingDirectory,
                                              exitCode, exitStatus, stdoutText, stderrText);
            if (reveal) {
                setCommandOutputVisible(true);
            }
        });
        connect(pane, &FilePane::terminalCommandStarted, this, [this](const QString &header) {
            setTerminalVisible(true);
            m_terminalPane->beginCommandOutput(header);
        });
        connect(pane, &FilePane::terminalCommandOutput, this, [this](const QString &chunk) {
            m_terminalPane->appendCommandOutput(chunk);
        });
        connect(pane, &FilePane::terminalCommandFinished, this, [this](const QString &footer) {
            m_terminalPane->endCommandOutput(footer);
        });
    };
    wirePane(m_leftPane);
    wirePane(m_rightPane);
    connect(m_terminalPane, &TerminalPane::closeRequested, this, [this]() {
        setTerminalVisible(false);
    });
    connect(m_terminalPane, &TerminalPane::directorySyncRequested, this, [this](const QString &path) {
        activePane()->navigateTo(path);
        statusBar()->showMessage(UiText::t("Synced pane to terminal directory.",
                                           "ペインをターミナルのディレクトリに同期しました。"), 3500);
    });

    buildActions();
    auto *focusOtherPaneShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(focusOtherPaneShortcut, &QShortcut::activated, this, &MainWindow::focusOtherPane);
    auto *focusPreviousPaneShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Tab), this);
    connect(focusPreviousPaneShortcut, &QShortcut::activated, this, &MainWindow::focusOtherPane);
    auto *togglePreviewSourceShortcut = new QShortcut(QKeySequence(m_config.shortcut("togglePreviewSource", "Ctrl+Shift+R")), this);
    connect(togglePreviewSourceShortcut, &QShortcut::activated, m_previewPane, &PreviewPane::toggleSourceRendered);
    auto *openPreviewExternalShortcut = new QShortcut(QKeySequence(m_config.shortcut("openPreviewExternal", "Ctrl+Shift+I")), this);
    connect(openPreviewExternalShortcut, &QShortcut::activated, m_previewPane, &PreviewPane::openCurrentPreviewExternally);

    // Documented action shortcuts that previously had no key binding.
    const auto addPaneShortcut = [this](const QString &name, const QString &def, void (FilePane::*slot)()) {
        auto *sc = new QShortcut(QKeySequence(m_config.shortcut(name, def)), this);
        connect(sc, &QShortcut::activated, this, [this, slot]() { (activePane()->*slot)(); });
    };
    addPaneShortcut("openItem", "Ctrl+O", &FilePane::openSelected);
    addPaneShortcut("openTerminal", "Ctrl+Shift+T", &FilePane::openTerminalHere);
    addPaneShortcut("compressToZip", "Ctrl+Alt+Z", &FilePane::compressSelectedItemsToZip);
    addPaneShortcut("extractZip", "Ctrl+Alt+E", &FilePane::extractSelectedZip);
    addPaneShortcut("selectAll", "Ctrl+A", &FilePane::selectAllVisibleItems);
    addPaneShortcut("revealInFinder", "Ctrl+Alt+R", &FilePane::revealSelectionInFileManager);
    addPaneShortcut("movePasteItems", "Ctrl+Shift+V", &FilePane::movePasteIntoCurrentDirectory);

    auto *swapPanesShortcut = new QShortcut(QKeySequence(m_config.shortcut("swapPanes", "Ctrl+Shift+X")), this);
    connect(swapPanesShortcut, &QShortcut::activated, this, &MainWindow::swapPanes);
    auto *focusTerminalShortcut = new QShortcut(QKeySequence(m_config.shortcut("focusTerminalPane", "Ctrl+Alt+J")), this);
    connect(focusTerminalShortcut, &QShortcut::activated, this, [this]() {
        setTerminalVisible(true);
        m_terminalPane->focusTerminal();
    });
    setActivePane(m_leftPane);
    m_previewPane->previewPath(initialPath);
    restoreSettings();

    auto *versionLabel = new QLabel(QString("v%1").arg(TFX_VERSION), this);
    versionLabel->setObjectName("statusVersion");
    m_fileOperationSummary = new QLabel(this);
    m_fileOperationSummary->setObjectName("fileOperationSummary");
    m_fileOperationSummary->hide();
    m_fileOperationProgress = new QProgressBar(this);
    m_fileOperationProgress->setRange(0, 100);
    m_fileOperationProgress->setFixedWidth(180);
    m_fileOperationProgress->setTextVisible(true);
    m_fileOperationProgress->hide();
    m_fileOperationCancel = new QPushButton(UiText::t("Cancel", "キャンセル"), this);
    m_fileOperationCancel->setObjectName("fileOperationCancel");
    m_fileOperationCancel->hide();
    connect(m_fileOperationCancel, &QPushButton::clicked, this, &MainWindow::cancelFileOperation);
    statusBar()->addPermanentWidget(m_fileOperationSummary);
    statusBar()->addPermanentWidget(m_fileOperationProgress);
    statusBar()->addPermanentWidget(m_fileOperationCancel);
    statusBar()->addPermanentWidget(versionLabel);

    QTimer::singleShot(0, this, [this]() {
        activePane()->focusFileList();
    });
    statusBar()->showMessage(initialPath);
}

void MainWindow::setIconViewEnabled(bool enabled)
{
    // View mode is per-pane: only the active file list changes.
    activePane()->setViewMode(enabled);
    syncIconViewToggle();
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::syncIconViewToggle()
{
    const bool on = activePane()->isIconMode();
    if (m_iconViewButton && m_iconViewButton->isChecked() != on) {
        const QSignalBlocker blocker(m_iconViewButton);
        m_iconViewButton->setChecked(on);
    }
    if (m_iconViewAction && m_iconViewAction->isChecked() != on) {
        const QSignalBlocker blocker(m_iconViewAction);
        m_iconViewAction->setChecked(on);
    }
}

void MainWindow::setActivePane(FilePane *pane)
{
    m_activePane = pane;
    m_leftPane->setActive(pane == m_leftPane);
    m_rightPane->setActive(pane == m_rightPane);
    syncIconViewToggle();

    const double inactiveAlpha = m_config.opacity.inactivePane;
    auto applyPaneOpacity = [inactiveAlpha](FilePane *p, bool active) {
        if (active || inactiveAlpha >= 1.0) {
            p->setGraphicsEffect(nullptr);
            return;
        }
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(p->graphicsEffect());
        if (!effect) {
            effect = new QGraphicsOpacityEffect(p);
            p->setGraphicsEffect(effect);
        }
        effect->setOpacity(inactiveAlpha);
    };
    applyPaneOpacity(m_leftPane, pane == m_leftPane);
    applyPaneOpacity(m_rightPane, pane == m_rightPane);

    m_searchEdit->clearEditText();
}

void MainWindow::focusOtherPane()
{
    if (!m_rightPane->isVisible()) {
        setActivePane(m_leftPane);
        m_leftPane->focusFileList();
        return;
    }

    FilePane *nextPane = activePane() == m_leftPane ? m_rightPane : m_leftPane;
    setActivePane(nextPane);
    nextPane->focusFileList();
}

void MainWindow::swapPanes()
{
    // Exchange the two panes' current folders. Only meaningful in split view.
    if (!m_rightPane->isVisible()) {
        return;
    }
    const QString leftPath = m_leftPane->currentPath();
    const QString rightPath = m_rightPane->currentPath();
    if (leftPath == rightPath) {
        return;
    }
    m_leftPane->navigateTo(rightPath);
    m_rightPane->navigateTo(leftPath);
}

FilePane *MainWindow::activePane() const
{
    return m_activePane ? m_activePane : m_leftPane;
}
