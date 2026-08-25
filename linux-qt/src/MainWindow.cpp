#include "MainWindow.h"
#include "views/SidebarViews.h"
#include "UiText.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileSystemModel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
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
#include <QWindow>

#include <unistd.h>

MainWindow::~MainWindow()
{
    // The QSocketNotifier is parented; only the mount-table fd is manual.
    if (m_mountsFd >= 0) {
        ::close(m_mountsFd);
        m_mountsFd = -1;
    }
}

MainWindow::MainWindow(const QString &initialPath, const QString &geometryOverride, QWidget *parent)
    : QMainWindow(parent),
      m_treeModel(new QFileSystemModel(this)),
      m_treeView(new FolderTreeView(this)),
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
    // Window flags must be set before the window is shown.
    m_integratedTitleBar = m_config.window.titleBar == QLatin1String("integrated");
    if (m_integratedTitleBar) {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
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
    // Clickable section headers: the chevron in the text shows the collapse
    // state; the state itself persists with the other view settings.
    const auto makeSectionHeader = [this](bool *state) {
        auto *header = new QToolButton(m_sidebar);
        header->setObjectName("sectionHeader");
        header->setToolButtonStyle(Qt::ToolButtonTextOnly);
        header->setAutoRaise(true);
        header->setCursor(Qt::PointingHandCursor);
        connect(header, &QToolButton::clicked, this, [this, state]() {
            *state = !*state;
            applySidebarSectionStates();
            if (!m_isRestoringSettings) {
                saveSettings();
            }
        });
        return header;
    };
    m_pinnedHeader = makeSectionHeader(&m_pinnedCollapsed);
    m_diskHeader = makeSectionHeader(&m_disksCollapsed);
    m_treeHeader = makeSectionHeader(&m_foldersCollapsed);
    auto *collapseTreeButton = new QToolButton(m_sidebar);
    collapseTreeButton->setObjectName("toolbarIconButton");
    collapseTreeButton->setText("-");
    collapseTreeButton->setToolTip(UiText::t("Collapse all folders", "すべてのフォルダーを折りたたむ"));
    collapseTreeButton->setFixedSize(24, 22);
    connect(collapseTreeButton, &QToolButton::clicked, this, &MainWindow::collapseFolderTree);
    auto *treeHeaderLayout = new QHBoxLayout();
    treeHeaderLayout->setContentsMargins(0, 0, 0, 0);
    treeHeaderLayout->addWidget(m_treeHeader);
    treeHeaderLayout->addStretch(1);
    treeHeaderLayout->addWidget(collapseTreeButton);
    sidebarLayout->addWidget(m_pinnedHeader);
    sidebarLayout->addWidget(m_pinnedList);
    sidebarLayout->addWidget(m_pinnedSpacer);
    sidebarLayout->addWidget(m_diskHeader);
    sidebarLayout->addWidget(m_diskList);
    sidebarLayout->addLayout(treeHeaderLayout);
    sidebarLayout->addWidget(m_treeView, 1);
    applySidebarSectionStates();

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
                syncFolderTree(path);
                updateDiskSelection(path);
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
    setupIntegratedTitleBar();
    setupConfigShortcuts();
    setupConfigWatcher();
    setActivePane(m_leftPane);
    m_previewPane->previewPath(initialPath);
    restoreSettings();
    showConfigWarnings();

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

// [window] titleBar = "integrated": the native title bar is hidden, the menu
// bar doubles as the drag handle (double-click maximizes), and the
// minimize/maximize/close controls live in the menu bar's corner. Window
// edges resize via startSystemResize; the status-bar size grip remains.
void MainWindow::setupIntegratedTitleBar()
{
    if (!m_integratedTitleBar) {
        return;
    }
    auto *controls = new QWidget(menuBar());
    auto *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(2);
    const auto makeButton = [this, controls](const QString &text, const QString &tip) {
        auto *button = new QToolButton(controls);
        button->setObjectName("toolbarIconButton");
        button->setText(text);
        button->setToolTip(tip);
        button->setFixedSize(26, 22);
        return button;
    };
    auto *minimizeButton = makeButton(QStringLiteral("–"), UiText::t("Minimize", "最小化"));
    connect(minimizeButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    m_maximizeButton = makeButton(QStringLiteral("□"), UiText::t("Maximize / restore", "最大化 / 元に戻す"));
    connect(m_maximizeButton, &QToolButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    auto *closeButton = makeButton(QStringLiteral("✕"), UiText::t("Close", "閉じる"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    layout->addWidget(minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(closeButton);
    menuBar()->setCornerWidget(controls, Qt::TopRightCorner);
    controls->show();

    // One application-level filter covers the menu-bar drag area and the
    // window-edge resize zones of every child widget.
    qApp->installEventFilter(this);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_integratedTitleBar) {
        return QMainWindow::eventFilter(watched, event);
    }
    if (watched == this && event->type() == QEvent::WindowStateChange && m_maximizeButton) {
        m_maximizeButton->setText(isMaximized() ? QString::fromUtf8("❐") : QStringLiteral("□"));
        return QMainWindow::eventFilter(watched, event);
    }
    if (watched == menuBar()
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton
            && !menuBar()->actionAt(mouse->position().toPoint())) {
            if (event->type() == QEvent::MouseButtonDblClick) {
                if (isMaximized()) {
                    showNormal();
                } else {
                    showMaximized();
                }
            } else if (windowHandle()) {
                windowHandle()->startSystemMove();
            }
            return true;
        }
        return QMainWindow::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseButtonPress && !isMaximized()) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        auto *widget = qobject_cast<QWidget *>(watched);
        if (mouse->button() == Qt::LeftButton && widget && widget->window() == this
            && windowHandle()) {
            const QPoint pos = mapFromGlobal(mouse->globalPosition().toPoint());
            const int margin = 6;
            Qt::Edges edges;
            if (pos.x() <= margin) {
                edges |= Qt::LeftEdge;
            }
            if (pos.x() >= width() - margin) {
                edges |= Qt::RightEdge;
            }
            if (pos.y() <= margin) {
                edges |= Qt::TopEdge;
            }
            if (pos.y() >= height() - margin) {
                edges |= Qt::BottomEdge;
            }
            if (edges) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// Creates every shortcut whose key comes from config.toml. Called at startup
// and again on each live config reload (the previous set is deleted first).
void MainWindow::setupConfigShortcuts()
{
    qDeleteAll(m_configShortcuts);
    m_configShortcuts.clear();
    const auto add = [this](const QKeySequence &sequence) {
        auto *shortcut = new QShortcut(sequence, this);
        m_configShortcuts.append(shortcut);
        return shortcut;
    };

    connect(add(QKeySequence(Qt::Key_Tab)), &QShortcut::activated, this, &MainWindow::focusOtherPane);
    connect(add(QKeySequence(Qt::SHIFT | Qt::Key_Tab)), &QShortcut::activated, this, &MainWindow::focusOtherPane);
    connect(add(QKeySequence(m_config.shortcut("togglePreviewSource", "Ctrl+Shift+R"))),
            &QShortcut::activated, m_previewPane, &PreviewPane::toggleSourceRendered);
    connect(add(QKeySequence(m_config.shortcut("openPreviewExternal", "Ctrl+Shift+I"))),
            &QShortcut::activated, m_previewPane, &PreviewPane::openCurrentPreviewExternally);

    const auto addPaneShortcut = [this, add](const QString &name, const QString &def, void (FilePane::*slot)()) {
        connect(add(QKeySequence(m_config.shortcut(name, def))), &QShortcut::activated,
                this, [this, slot]() { (activePane()->*slot)(); });
    };
    addPaneShortcut("openItem", "Ctrl+O", &FilePane::openSelected);
    addPaneShortcut("openTerminal", "Ctrl+Shift+T", &FilePane::openTerminalHere);
    addPaneShortcut("compressToZip", "Ctrl+Alt+Z", &FilePane::compressSelectedItemsToZip);
    addPaneShortcut("extractZip", "Ctrl+Alt+E", &FilePane::extractSelectedZip);
    addPaneShortcut("selectAll", "Ctrl+A", &FilePane::selectAllVisibleItems);
    addPaneShortcut("revealInFinder", "Ctrl+Alt+R", &FilePane::revealSelectionInFileManager);
    addPaneShortcut("movePasteItems", "Ctrl+Shift+V", &FilePane::movePasteIntoCurrentDirectory);

    connect(add(QKeySequence(m_config.shortcut("swapPanes", "Ctrl+Shift+X"))),
            &QShortcut::activated, this, &MainWindow::swapPanes);
    connect(add(QKeySequence(m_config.shortcut("focusTerminalPane", "Ctrl+Alt+J"))),
            &QShortcut::activated, this, [this]() {
                setTerminalVisible(true);
                m_terminalPane->focusTerminal();
            });
    connect(add(QKeySequence(m_config.shortcut("focusSearch", "Ctrl+F"))),
            &QShortcut::activated, m_searchEdit, [this]() {
                m_searchEdit->setFocus();
                m_searchEdit->lineEdit()->selectAll();
            });
    // editConfig and sortOptions are bound through their menu actions (a
    // duplicate QShortcut
    // would make the key ambiguous and fire neither).
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
