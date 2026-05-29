#include "MainWindow.h"
#include "UiText.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
QAction *addMenuAction(QMenu *menu,
                       const QString &text,
                       QObject *receiver,
                       const std::function<void()> &slot,
                       const QKeySequence &shortcut = QKeySequence())
{
    auto *action = menu->addAction(text);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    QObject::connect(action, &QAction::triggered, receiver, slot);
    return action;
}

QIcon toolbarIcon(const QString &kind)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#CFFFCF"), 1.7);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (kind == "split") {
        QRectF rect(7, 8, 18, 16);
        painter.drawRoundedRect(rect, 2, 2);
        painter.drawLine(QPointF(16, 8), QPointF(16, 24));
    } else if (kind == "preview") {
        QRectF rect(7, 8, 18, 16);
        painter.drawRoundedRect(rect, 2, 2);
        painter.fillRect(QRectF(18, 9, 6, 14), QColor("#CFFFCF"));
        painter.drawLine(QPointF(18, 8), QPointF(18, 24));
    } else if (kind == "search") {
        painter.drawEllipse(QRectF(9, 9, 10, 10));
        painter.drawLine(QPointF(17, 17), QPointF(23, 23));
    }

    return QIcon(pixmap);
}
}

MainWindow::MainWindow(const QString &initialPath, QWidget *parent)
    : QMainWindow(parent),
      m_treeModel(new QFileSystemModel(this)),
      m_treeView(new QTreeView(this)),
      m_pinnedList(new QListWidget(this)),
      m_pinnedSpacer(new QWidget(this)),
      m_searchEdit(new QLineEdit(this)),
      m_topToolbar(new QToolBar(this)),
      m_splitButton(new QToolButton(this)),
      m_previewButton(new QToolButton(this)),
      m_sidebar(new QWidget(this)),
      m_leftPane(new FilePane("LEFT", initialPath, this)),
      m_rightPane(new FilePane("RIGHT", QDir::homePath(), this)),
      m_activePane(m_leftPane),
      m_previewPane(new PreviewPane(this)),
      m_terminalPane(new TerminalPane(this)),
      m_fileSplitter(new QSplitter(Qt::Horizontal, this)),
      m_mainSplitter(new QSplitter(Qt::Horizontal, this)),
      m_verticalSplitter(new QSplitter(Qt::Vertical, this))
{
    setWindowTitle("tfx Qt");
    resize(1280, 780);
    applyTerminalTheme();
    buildTopToolbar();
    buildFolderSidebar(initialPath);

    m_fileSplitter->setObjectName("fileSplitter");
    m_mainSplitter->setObjectName("mainSplitter");
    m_verticalSplitter->setObjectName("verticalSplitter");
    m_fileSplitter->setHandleWidth(6);
    m_mainSplitter->setHandleWidth(5);
    m_verticalSplitter->setHandleWidth(5);
    m_sidebar->setMinimumWidth(160);
    m_treeView->setMinimumWidth(140);
    m_pinnedList->setMinimumWidth(140);

    m_fileSplitter->addWidget(m_leftPane);
    m_fileSplitter->addWidget(m_rightPane);
    m_fileSplitter->setStretchFactor(0, 1);
    m_fileSplitter->setStretchFactor(1, 1);
    m_fileSplitter->setCollapsible(0, false);
    m_fileSplitter->setCollapsible(1, false);

    m_sidebar->setObjectName("sidebar");
    auto *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(10, 8, 8, 0);
    sidebarLayout->setSpacing(6);
    auto *pinnedLabel = new QLabel(UiText::t("FOLDERS", "フォルダ"), m_sidebar);
    auto *treeLabel = new QLabel(UiText::t("FOLDERS", "フォルダ"), m_sidebar);
    pinnedLabel->setObjectName("sectionLabel");
    treeLabel->setObjectName("sectionLabel");
    sidebarLayout->addWidget(pinnedLabel);
    sidebarLayout->addWidget(m_pinnedList);
    sidebarLayout->addWidget(m_pinnedSpacer);
    sidebarLayout->addWidget(treeLabel);
    sidebarLayout->addWidget(m_treeView, 1);

    m_mainSplitter->addWidget(m_sidebar);
    m_mainSplitter->addWidget(m_fileSplitter);
    m_mainSplitter->addWidget(m_previewPane);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setCollapsible(2, false);
    syncLayoutConstraints();

    m_verticalSplitter->addWidget(m_mainSplitter);
    m_verticalSplitter->addWidget(m_terminalPane);
    m_verticalSplitter->setStretchFactor(0, 1);
    m_verticalSplitter->setStretchFactor(1, 0);
    m_terminalPane->hide();
    setCentralWidget(m_verticalSplitter);

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
        connect(pane, &FilePane::statusMessageRequested, this, [this](const QString &message) {
            statusBar()->showMessage(message, 3500);
        });
        connect(pane, &FilePane::pinFolderRequested, this, &MainWindow::addPinnedFolder);
        connect(pane, &FilePane::openTerminalHereRequested, this, [this](const QString &path) {
            m_terminalPane->setWorkingDirectory(path);
            m_terminalPane->show();
        });
    };
    wirePane(m_leftPane);
    wirePane(m_rightPane);
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, [this]() {
        rememberSidebarWidth();
        rememberPreviewWidth();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    });
    connect(m_fileSplitter, &QSplitter::splitterMoved, this, [this]() {
        rememberSplitWidth();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    });
    connect(m_verticalSplitter, &QSplitter::splitterMoved, this, [this]() {
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    });

    buildActions();
    setActivePane(m_leftPane);
    m_previewPane->previewPath(initialPath);
    restoreSettings();
    statusBar()->showMessage(initialPath);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildActions()
{
    auto *fileMenu = menuBar()->addMenu(UiText::t("File", "ファイル"));
    addMenuAction(fileMenu, UiText::t("New File", "新規ファイル"), this, [this]() { activePane()->createFile(); }, QKeySequence("Ctrl+Shift+N"));
    addMenuAction(fileMenu, UiText::t("New Folder", "新規フォルダ"), this, [this]() { activePane()->createFolder(); }, QKeySequence("Ctrl+N"));
    addMenuAction(fileMenu, UiText::t("Rename", "名前を変更"), this, [this]() { activePane()->renameSelected(); }, QKeySequence("F2"));
    addMenuAction(fileMenu, UiText::t("Move to Trash", "ゴミ箱へ移動"), this, [this]() { activePane()->moveSelectedToTrash(); }, QKeySequence("Del"));
    fileMenu->addSeparator();
    addMenuAction(fileMenu, UiText::t("Quit", "終了"), qApp, []() { QApplication::quit(); }, QKeySequence("Ctrl+Q"));

    auto *editMenu = menuBar()->addMenu(UiText::t("Edit", "編集"));
    addMenuAction(editMenu, UiText::t("Copy", "コピー"), this, [this]() { activePane()->copySelected(); }, QKeySequence::Copy);
    addMenuAction(editMenu, UiText::t("Cut", "カット"), this, [this]() { activePane()->cutSelected(); }, QKeySequence::Cut);
    addMenuAction(editMenu, UiText::t("Paste", "ペースト"), this, [this]() { activePane()->pasteIntoCurrentDirectory(); }, QKeySequence::Paste);
    addMenuAction(editMenu, UiText::t("Copy Path", "パスをコピー"), this, [this]() { activePane()->copySelectedPaths(); }, QKeySequence("Ctrl+Shift+C"));

    auto *viewMenu = menuBar()->addMenu(UiText::t("View", "表示"));
    m_splitAction = viewMenu->addAction(UiText::t("Split Pane", "スプリット表示"));
    m_splitAction->setCheckable(true);
    m_splitAction->setChecked(true);
    m_splitAction->setShortcut(QKeySequence("Ctrl+\\"));
    connect(m_splitAction, &QAction::toggled, this, &MainWindow::setSplitVisible);

    m_previewAction = viewMenu->addAction(UiText::t("Preview", "プレビュー"));
    m_previewAction->setCheckable(true);
    m_previewAction->setChecked(true);
    m_previewAction->setShortcut(QKeySequence("Ctrl+P"));
    connect(m_previewAction, &QAction::toggled, this, &MainWindow::setPreviewVisible);

    m_terminalAction = viewMenu->addAction(UiText::t("Terminal Pane", "ターミナルペイン"));
    m_terminalAction->setCheckable(true);
    m_terminalAction->setShortcut(QKeySequence("Ctrl+Alt+T"));
    connect(m_terminalAction, &QAction::toggled, this, &MainWindow::setTerminalVisible);

    m_hiddenAction = viewMenu->addAction(UiText::t("Show Hidden Files", "隠しファイルを表示"));
    m_hiddenAction->setCheckable(true);
    m_hiddenAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(m_hiddenAction, &QAction::toggled, this, &MainWindow::setHiddenFilesVisible);

    addMenuAction(viewMenu, UiText::t("File List Settings...", "ファイル一覧設定..."), this, [this]() {
        activePane()->showColumnSettingsDialog();
    });

    auto *tabMenu = menuBar()->addMenu(UiText::t("Tabs", "タブ"));
    addMenuAction(tabMenu, UiText::t("New Tab", "新規タブ"), this, [this]() { activePane()->newTab(); }, QKeySequence("Ctrl+Shift+T"));
    addMenuAction(tabMenu, UiText::t("Close Tab", "タブを閉じる"), this, [this]() { activePane()->closeCurrentTab(); }, QKeySequence("Ctrl+W"));
    addMenuAction(tabMenu, UiText::t("Previous Tab", "前のタブ"), this, [this]() { activePane()->previousTab(); }, QKeySequence("Ctrl+Shift+["));
    addMenuAction(tabMenu, UiText::t("Next Tab", "次のタブ"), this, [this]() { activePane()->nextTab(); }, QKeySequence("Ctrl+Shift+]"));

    auto *navMenu = menuBar()->addMenu(UiText::t("Navigate", "移動"));
    addMenuAction(navMenu, UiText::t("Parent", "親フォルダ"), this, [this]() { activePane()->goUp(); }, QKeySequence("Alt+Up"));
    addMenuAction(navMenu, UiText::t("Back", "戻る"), this, [this]() { activePane()->goBack(); }, QKeySequence("Alt+Left"));
    addMenuAction(navMenu, UiText::t("Forward", "進む"), this, [this]() { activePane()->goForward(); }, QKeySequence("Alt+Right"));
    addMenuAction(navMenu, UiText::t("Reload", "再読み込み"), this, [this]() { activePane()->reload(); }, QKeySequence("Ctrl+R"));
}

void MainWindow::buildTopToolbar()
{
    m_topToolbar->setObjectName("topToolbar");
    m_topToolbar->setMovable(false);
    m_topToolbar->setFloatable(false);
    m_topToolbar->setIconSize(QSize(18, 18));
    addToolBar(Qt::TopToolBarArea, m_topToolbar);

    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(UiText::t("Search", "検索"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(260);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        activePane()->setPathFilter(text);
    });
    m_topToolbar->addWidget(m_searchEdit);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_topToolbar->addWidget(spacer);

    m_splitButton->setObjectName("toolbarIconButton");
    m_splitButton->setIcon(toolbarIcon("split"));
    m_splitButton->setIconSize(QSize(24, 24));
    m_splitButton->setCheckable(true);
    m_splitButton->setChecked(true);
    m_splitButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_splitButton->setToolTip(UiText::t("Use single pane / split panes", "単独表示 / スプリット表示"));
    connect(m_splitButton, &QToolButton::toggled, this, &MainWindow::setSplitVisible);
    m_topToolbar->addWidget(m_splitButton);

    m_previewButton->setObjectName("toolbarIconButton");
    m_previewButton->setIcon(toolbarIcon("preview"));
    m_previewButton->setIconSize(QSize(24, 24));
    m_previewButton->setCheckable(true);
    m_previewButton->setChecked(true);
    m_previewButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_previewButton->setToolTip(UiText::t("Show / hide preview", "プレビューを表示 / 非表示"));
    connect(m_previewButton, &QToolButton::toggled, this, &MainWindow::setPreviewVisible);
    m_topToolbar->addWidget(m_previewButton);
}

void MainWindow::buildFolderSidebar(const QString &initialPath)
{
    m_treeModel->setRootPath("/");
    m_treeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_treeView->setModel(m_treeModel);
    m_treeView->setRootIndex(m_treeModel->index("/"));
    for (int column = 1; column < m_treeModel->columnCount(); ++column) {
        m_treeView->hideColumn(column);
    }
    m_treeView->header()->hide();
    m_treeView->setCurrentIndex(m_treeModel->index(initialPath));
    m_treeView->setIndentation(18);
    m_treeView->setIconSize(QSize(18, 18));
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        const QString path = m_treeModel->filePath(index);
        activePane()->navigateTo(path);
    });
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &point) {
        const QModelIndex index = m_treeView->indexAt(point);
        if (!index.isValid()) {
            return;
        }
        const QString path = m_treeModel->filePath(index);
        QMenu menu(this);
        menu.addAction(UiText::t("Open", "開く"), this, [this, path]() {
            activePane()->navigateTo(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, path]() {
            addPinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->setWorkingDirectory(path);
            m_terminalPane->show();
        });
        menu.exec(m_treeView->viewport()->mapToGlobal(point));
    });

    const QStringList pinnedPaths = {
        QDir::homePath(),
        QDir::home().filePath("Desktop"),
        QDir::home().filePath("Documents"),
        QDir::home().filePath("Downloads"),
        QDir::home().filePath("Pictures"),
    };
    for (const QString &path : pinnedPaths) {
        addPinnedFolder(path);
    }
    m_pinnedList->setUniformItemSizes(true);
    m_pinnedList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pinnedList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_pinnedList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        activePane()->navigateTo(item->data(Qt::UserRole).toString());
    });
    m_pinnedList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pinnedList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QListWidgetItem *item = m_pinnedList->itemAt(point);
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        QMenu menu(this);
        menu.addAction(UiText::t("Open", "開く"), this, [this, path]() {
            activePane()->navigateTo(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Unpin Folder", "ピン留めを解除"), this, [this, path]() {
            removePinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->setWorkingDirectory(path);
            m_terminalPane->show();
        });
        menu.exec(m_pinnedList->viewport()->mapToGlobal(point));
    });
}

void MainWindow::applyTerminalTheme()
{
    qApp->setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #000301;
            color: #CFFFCF;
            font-family: "JetBrains Mono", "Fira Code", "DejaVu Sans Mono", monospace;
            font-size: 13px;
        }
        QMenuBar, QMenu {
            background: #030905;
            color: #CFFFCF;
            border: 1px solid #102E1A;
        }
        QMenu::item { padding: 5px 22px 5px 18px; }
        QMenu::item:selected { background: #102E1A; color: #6FFF80; }
        QToolBar#topToolbar {
            background: #030905;
            border: 0;
            border-bottom: 1px solid #0B2012;
            padding: 7px 9px;
            spacing: 8px;
        }
        QLineEdit#pathEdit {
            background: #020704;
            color: #CFFFCF;
            border: 1px solid #0B2012;
            border-radius: 6px;
            padding: 8px 14px;
            font-weight: 700;
        }
        QLineEdit#searchEdit {
            background: #061109;
            color: #CFFFCF;
            border: 1px solid #102E1A;
            border-radius: 6px;
            padding: 7px 11px;
            selection-background-color: #102E1A;
        }
        QWidget#sidebar {
            background: #000301;
            border-right: 1px solid #102E1A;
        }
        QLabel#sectionLabel {
            color: #6FFF80;
            font-weight: 700;
            padding: 6px 0 4px 0;
        }
        QListWidget, QTreeView {
            background: #000301;
            color: #CFFFCF;
            selection-background-color: #0B2012;
            selection-color: #6FFF80;
            border: 0;
            outline: 0;
        }
        QTableView#fileTable {
            background: #000301;
            color: #CFFFCF;
            selection-background-color: #102E1A;
            selection-color: #FFFFFF;
            border: 0;
            outline: 0;
        }
        QHeaderView::section {
            background: #000301;
            color: #6FFF80;
            border: 0;
            border-top: 1px solid #061109;
            border-bottom: 1px solid #061109;
            padding: 4px 10px;
            font-weight: 700;
        }
        QTabBar#paneTabs {
            background: #000301;
            border-bottom: 1px solid #0B2012;
            min-height: 21px;
        }
        QTabBar#paneTabs::tab {
            background: transparent;
            color: #1A8F39;
            border: 1px solid transparent;
            border-radius: 2px;
            padding: 2px 10px;
            margin: 1px 2px 1px 0;
        }
        QTabBar#paneTabs::tab:selected {
            color: #6FFF80;
            border-color: #125625;
            background: #061109;
        }
        QWidget#filePane {
            background: #000301;
            border: 1px solid #0B2012;
        }
        QWidget#filePane[activePane="true"] {
            border: 2px solid #6FFF80;
        }
        QLabel#paneBadge {
            color: #6FFF80;
            background: transparent;
            border: 1px solid #125625;
            border-radius: 2px;
            padding: 2px 8px;
            font-weight: 700;
        }
        QLabel#paneBadge[activePane="true"] {
            color: #001909;
            background: #6FFF80;
            border-color: #6FFF80;
        }
        QLabel#panePath {
            color: #1A8F39;
            font-weight: 600;
        }
        QLabel#panePath[activePane="true"] {
            color: #6FFF80;
        }
        QLabel#paneStatus {
            background: #030905;
            color: #6FFF80;
            border-top: 1px solid #0B2012;
            padding: 7px 10px;
            font-weight: 600;
        }
        QToolButton, QPushButton {
            background: #061109;
            color: #CFFFCF;
            border: 1px solid #102E1A;
            border-radius: 6px;
            padding: 5px 8px;
        }
        QToolButton#toolbarIconButton {
            min-width: 36px;
            max-width: 36px;
            min-height: 28px;
            max-height: 28px;
            padding: 0;
            background: #061109;
        }
        QToolButton#previewSourceToggle {
            min-width: 32px;
            max-width: 32px;
            min-height: 24px;
            max-height: 24px;
            padding: 0;
            background: #061109;
        }
        QToolButton:hover, QPushButton:hover {
            background: #0B2012;
            border-color: #125625;
        }
        QToolButton#toolbarIconButton:checked,
        QToolButton#previewSourceToggle:checked {
            background: #102E1A;
            border-color: #6FFF80;
            color: #CFFFCF;
        }
        QSplitter::handle {
            background: #0B2012;
        }
        QSplitter#fileSplitter::handle {
            background: #061109;
            border-left: 1px solid #125625;
            border-right: 1px solid #125625;
        }
        QSplitter#mainSplitter::handle,
        QSplitter#verticalSplitter::handle {
            background: #061109;
            border: 1px solid #0B2012;
        }
        QStatusBar {
            background: #030905;
            color: #1A8F39;
            border-top: 1px solid #0B2012;
        }
        QPlainTextEdit#previewCode, QTextBrowser#previewRendered {
            background: #000301;
            color: #CFFFCF;
            border: 0;
            selection-background-color: #102E1A;
        }
        QLabel#previewTitle {
            color: #1A8F39;
            font-weight: 600;
        }
    )");
}

void MainWindow::addPinnedFolder(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isDir()) {
        return;
    }
    const QString cleanPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        if (m_pinnedList->item(row)->data(Qt::UserRole).toString() == cleanPath) {
            return;
        }
    }

    auto *item = new QListWidgetItem(info.fileName().isEmpty() ? cleanPath : info.fileName());
    item->setSizeHint(QSize(0, 18));
    item->setData(Qt::UserRole, cleanPath);
    m_pinnedList->addItem(item);
    updatePinnedFolderArea();
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::removePinnedFolder(const QString &path)
{
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        QListWidgetItem *item = m_pinnedList->item(row);
        if (item->data(Qt::UserRole).toString() == path) {
            delete m_pinnedList->takeItem(row);
            updatePinnedFolderArea();
            if (!m_isRestoringSettings) {
                saveSettings();
            }
            return;
        }
    }
}

void MainWindow::restoreSettings()
{
    m_isRestoringSettings = true;
    QSettings settings;
    const bool splitVisible = settings.value("View/splitVisible", true).toBool();
    const bool previewVisible = settings.value("View/previewVisible", true).toBool();
    const bool terminalVisible = settings.value("View/terminalVisible", false).toBool();

    const QByteArray geometry = settings.value("MainWindow/geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray state = settings.value("MainWindow/state").toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    m_rightPane->setVisible(splitVisible);
    m_previewPane->setVisible(previewVisible);
    m_mainSplitter->setCollapsible(2, !previewVisible);
    m_terminalPane->setVisible(terminalVisible);
    syncLayoutConstraints();

    const QByteArray mainSplitterState = settings.value("MainWindow/mainSplitter").toByteArray();
    if (!mainSplitterState.isEmpty()) {
        m_mainSplitter->restoreState(mainSplitterState);
    }
    m_lastSidebarWidth = settings.value("Sidebar/width", m_lastSidebarWidth).toInt();
    m_lastPreviewWidth = settings.value("View/previewWidth", m_lastPreviewWidth).toInt();
    const QByteArray fileSplitterState = settings.value("MainWindow/fileSplitter").toByteArray();
    if (!fileSplitterState.isEmpty()) {
        m_fileSplitter->restoreState(fileSplitterState);
    }
    m_lastSplitWidth = settings.value("View/splitWidth", m_lastSplitWidth).toInt();
    const QByteArray verticalSplitterState = settings.value("MainWindow/verticalSplitter").toByteArray();
    if (!verticalSplitterState.isEmpty()) {
        m_verticalSplitter->restoreState(verticalSplitterState);
    }
    syncLayoutConstraints();

    const QString rightDirectory = settings.value("Panes/rightDirectory", m_rightPane->currentPath()).toString();
    if (QFileInfo(rightDirectory).isDir()) {
        m_rightPane->navigateTo(rightDirectory, false);
    }
    m_leftPane->restoreTabs(
        settings.value("Tabs/leftPaths").toStringList(),
        settings.value("Tabs/leftActiveIndex", 0).toInt());
    m_rightPane->restoreTabs(
        settings.value("Tabs/rightPaths").toStringList(),
        settings.value("Tabs/rightActiveIndex", 0).toInt());

    const QStringList pinnedPaths = settings.value("Sidebar/pinnedFolders").toStringList();
    if (!pinnedPaths.isEmpty()) {
        m_pinnedList->clear();
        for (const QString &path : pinnedPaths) {
            addPinnedFolder(path);
        }
        updatePinnedFolderArea();
    }

    setHiddenFilesVisible(settings.value("View/showHiddenFiles", false).toBool());
    {
        const QSignalBlocker splitButtonBlocker(m_splitButton);
        m_splitButton->setChecked(splitVisible);
    }
    if (m_splitAction) {
        const QSignalBlocker splitActionBlocker(m_splitAction);
        m_splitAction->setChecked(splitVisible);
    }
    {
        const QSignalBlocker previewButtonBlocker(m_previewButton);
        m_previewButton->setChecked(previewVisible);
    }
    if (m_previewAction) {
        const QSignalBlocker previewActionBlocker(m_previewAction);
        m_previewAction->setChecked(previewVisible);
    }
    if (m_terminalAction) {
        const QSignalBlocker terminalActionBlocker(m_terminalAction);
        m_terminalAction->setChecked(terminalVisible);
    }

    const QString activePaneName = settings.value("Panes/activePane", "LEFT").toString();
    setActivePane(activePaneName == "RIGHT" ? m_rightPane : m_leftPane);
    m_isRestoringSettings = false;
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/state", saveState());
    settings.setValue("MainWindow/mainSplitter", m_mainSplitter->saveState());
    settings.setValue("MainWindow/fileSplitter", m_fileSplitter->saveState());
    settings.setValue("MainWindow/verticalSplitter", m_verticalSplitter->saveState());
    rememberSidebarWidth();
    settings.setValue("Sidebar/width", m_lastSidebarWidth);
    settings.setValue("View/splitVisible", m_rightPane->isVisible());
    rememberSplitWidth();
    settings.setValue("View/splitWidth", m_lastSplitWidth);
    settings.setValue("View/previewVisible", m_previewPane->isVisible());
    rememberPreviewWidth();
    settings.setValue("View/previewWidth", m_lastPreviewWidth);
    settings.setValue("View/terminalVisible", m_terminalPane->isVisible());
    settings.setValue("View/showHiddenFiles", m_showHiddenFiles);
    settings.setValue("Panes/activePane", activePane() == m_rightPane ? "RIGHT" : "LEFT");
    settings.setValue("Panes/leftDirectory", m_leftPane->currentPath());
    settings.setValue("Panes/rightDirectory", m_rightPane->currentPath());
    settings.setValue("Tabs/leftPaths", m_leftPane->tabPaths());
    settings.setValue("Tabs/rightPaths", m_rightPane->tabPaths());
    settings.setValue("Tabs/leftActiveIndex", m_leftPane->activeTabIndex());
    settings.setValue("Tabs/rightActiveIndex", m_rightPane->activeTabIndex());

    QStringList pinnedPaths;
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        pinnedPaths.append(m_pinnedList->item(row)->data(Qt::UserRole).toString());
    }
    settings.setValue("Sidebar/pinnedFolders", pinnedPaths);
}

void MainWindow::rememberSidebarWidth()
{
    const QList<int> sizes = m_mainSplitter->sizes();
    if (sizes.size() >= 3 && sizes.at(0) > 80) {
        m_lastSidebarWidth = sizes.at(0);
    }
}

int MainWindow::fileAreaMinimumWidth(bool splitVisible) const
{
    return splitVisible
        ? m_leftPane->minimumWidth() + m_fileSplitter->handleWidth() + m_rightPane->minimumWidth()
        : m_leftPane->minimumWidth();
}

int MainWindow::contentMinimumWidth(bool splitVisible, bool previewVisible) const
{
    int width = m_sidebar->minimumWidth() + m_mainSplitter->handleWidth() + fileAreaMinimumWidth(splitVisible);
    if (previewVisible) {
        width += m_mainSplitter->handleWidth() + m_previewPane->minimumWidth();
    }
    return width;
}

int MainWindow::visibleFileListWidth() const
{
    const QList<int> sizes = m_fileSplitter->sizes();
    if (!sizes.isEmpty() && sizes.at(0) > 0) {
        return std::max(m_leftPane->minimumWidth(), sizes.at(0));
    }
    return std::max(m_leftPane->minimumWidth(), m_fileSplitter->width());
}

void MainWindow::syncLayoutConstraints()
{
    const bool splitVisible = m_rightPane->isVisible();
    const bool previewVisible = m_previewPane->isVisible();

    m_fileSplitter->setMinimumWidth(fileAreaMinimumWidth(splitVisible));
    m_fileSplitter->setCollapsible(0, false);
    m_fileSplitter->setCollapsible(1, !splitVisible);

    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setCollapsible(2, !previewVisible);

    setMinimumWidth(contentMinimumWidth(splitVisible, previewVisible));
}

QList<int> MainWindow::normalizedMainSplitterSizes(int sidebarWidth, int fileWidth, int previewWidth) const
{
    const bool previewVisible = m_previewPane->isVisible();
    return {
        std::max(m_sidebar->minimumWidth(), sidebarWidth),
        std::max(fileAreaMinimumWidth(m_rightPane->isVisible()), fileWidth),
        previewVisible ? std::max(m_previewPane->minimumWidth(), previewWidth) : 0,
    };
}

QList<int> MainWindow::normalizedFileSplitterSizes(int leftWidth, int rightWidth) const
{
    const bool splitVisible = m_rightPane->isVisible();
    return {
        std::max(m_leftPane->minimumWidth(), leftWidth),
        splitVisible ? std::max(m_rightPane->minimumWidth(), rightWidth) : 0,
    };
}

void MainWindow::setSplitVisible(bool visible)
{
    const bool wasVisible = m_rightPane->isVisible();
    const int windowWidthBefore = width();
    const QList<int> fileSizes = m_fileSplitter->sizes();
    const QList<int> mainSizes = m_mainSplitter->sizes();
    const int sidebarWidth = std::max(m_sidebar->minimumWidth(), mainSizes.value(0, m_sidebar->width()));
    const int fileAreaWidth = std::max(fileAreaMinimumWidth(wasVisible), mainSizes.value(1, visibleFileListWidth()));
    const int previewWidth = mainSizes.size() >= 3 ? std::max(0, mainSizes.value(2, 0)) : 0;
    const int leftWidth = visibleFileListWidth();
    const int rightWidth = std::max(0, fileSizes.value(1, 0));

    if (visible && !wasVisible) {
        const int targetRightWidth = std::max(m_rightPane->minimumWidth(), leftWidth);
        const int targetFileAreaWidth = leftWidth + m_fileSplitter->handleWidth() + targetRightWidth;
        const int currentFileAreaWidth = std::max(fileAreaWidth, leftWidth);
        const int addedWidth = std::max(0, targetFileAreaWidth - currentFileAreaWidth);
        m_rightPane->setVisible(true);
        syncLayoutConstraints();
        if (!m_isRestoringSettings && addedWidth > 0) {
            resize(windowWidthBefore + addedWidth, height());
        }
        m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, targetFileAreaWidth, previewWidth));
        m_fileSplitter->setSizes(normalizedFileSplitterSizes(leftWidth, targetRightWidth));
        m_lastSplitWidth = targetRightWidth;
    } else if (!visible && wasVisible) {
        if (rightWidth > 80) {
            m_lastSplitWidth = rightWidth;
        }
        const int targetLeftWidth = std::max(m_leftPane->minimumWidth(), leftWidth);
        const int removedWidth = std::max(0, fileAreaWidth - targetLeftWidth);
        m_rightPane->setVisible(false);
        syncLayoutConstraints();
        if (!m_isRestoringSettings && removedWidth > 0) {
            resize(std::max(minimumWidth(), windowWidthBefore - removedWidth), height());
        }
        m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, targetLeftWidth, previewWidth));
        m_fileSplitter->setSizes(normalizedFileSplitterSizes(targetLeftWidth, 0));
    } else {
        syncLayoutConstraints();
    }

    const QSignalBlocker splitButtonBlocker(m_splitButton);
    if (m_splitButton->isChecked() != visible) {
        m_splitButton->setChecked(visible);
    }
    if (m_splitAction && m_splitAction->isChecked() != visible) {
        const QSignalBlocker splitActionBlocker(m_splitAction);
        m_splitAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::rememberSplitWidth()
{
    if (!m_rightPane->isVisible()) {
        return;
    }

    const QList<int> sizes = m_fileSplitter->sizes();
    if (sizes.size() >= 2 && sizes.at(1) > 80) {
        m_lastSplitWidth = sizes.at(1);
    }
}

void MainWindow::setPreviewVisible(bool visible)
{
    const bool wasVisible = m_previewPane->isVisible();
    const int windowWidthBefore = width();
    const QList<int> mainSizes = m_mainSplitter->sizes();
    const int sidebarWidth = std::max(m_sidebar->minimumWidth(), mainSizes.value(0, m_sidebar->width()));
    const int fileWidth = std::max(fileAreaMinimumWidth(m_rightPane->isVisible()), mainSizes.value(1, m_fileSplitter->width()));
    const int previewWidth = std::max(0, mainSizes.value(2, 0));

    if (visible && !wasVisible) {
        const int targetPreviewWidth = std::max(m_previewPane->minimumWidth(), m_lastPreviewWidth);
        m_previewPane->setVisible(true);
        syncLayoutConstraints();
        if (!m_isRestoringSettings) {
            resize(windowWidthBefore + targetPreviewWidth, height());
        }
        m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, fileWidth, targetPreviewWidth));
    } else if (!visible && wasVisible) {
        if (previewWidth > 80) {
            m_lastPreviewWidth = previewWidth;
        }
        m_previewPane->setVisible(false);
        syncLayoutConstraints();
        m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, fileWidth, 0));
        if (!m_isRestoringSettings && previewWidth > 0) {
            resize(std::max(minimumWidth(), windowWidthBefore - previewWidth), height());
            m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, fileWidth, 0));
        }
    } else {
        syncLayoutConstraints();
    }

    const QSignalBlocker previewButtonBlocker(m_previewButton);
    if (m_previewButton->isChecked() != visible) {
        m_previewButton->setChecked(visible);
    }
    if (m_previewAction && m_previewAction->isChecked() != visible) {
        const QSignalBlocker previewActionBlocker(m_previewAction);
        m_previewAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::rememberPreviewWidth()
{
    if (!m_previewPane->isVisible()) {
        return;
    }

    const QList<int> sizes = m_mainSplitter->sizes();
    if (sizes.size() >= 3 && sizes.at(2) > 80) {
        m_lastPreviewWidth = sizes.at(2);
    }
}

void MainWindow::setTerminalVisible(bool visible)
{
    m_terminalPane->setVisible(visible);
    if (m_terminalAction && m_terminalAction->isChecked() != visible) {
        m_terminalAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setHiddenFilesVisible(bool visible)
{
    m_showHiddenFiles = visible;
    m_leftPane->setShowHiddenFiles(visible);
    m_rightPane->setShowHiddenFiles(visible);
    QDir::Filters filters = QDir::AllDirs | QDir::NoDotAndDotDot;
    if (visible) {
        filters |= QDir::Hidden | QDir::System;
    }
    m_treeModel->setFilter(filters);
    if (m_hiddenAction && m_hiddenAction->isChecked() != visible) {
        m_hiddenAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::updatePinnedFolderArea()
{
    const int pinnedCount = m_pinnedList->count();
    const int rowHeight = 18;
    const int frameHeight = m_pinnedList->frameWidth() * 2;
    const int listHeight = pinnedCount > 0 ? (rowHeight * pinnedCount) + frameHeight + 2 : 0;
    m_pinnedList->setFixedHeight(listHeight);
    m_pinnedList->setVisible(pinnedCount > 0);

    const int spacerHeight = pinnedCount > 0 ? 6 : 4;
    m_pinnedSpacer->setFixedHeight(spacerHeight);
}

void MainWindow::setActivePane(FilePane *pane)
{
    m_activePane = pane;
    m_leftPane->setActive(pane == m_leftPane);
    m_rightPane->setActive(pane == m_rightPane);
    m_searchEdit->clear();
}

FilePane *MainWindow::activePane() const
{
    return m_activePane ? m_activePane : m_leftPane;
}
