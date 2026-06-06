#include "MainWindow.h"
#include "UiText.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QGraphicsOpacityEffect>
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
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
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

QString withAlpha(const QString &color, double alpha)
{
    const QColor c(color);
    if (!c.isValid()) {
        return color;
    }
    return QString("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(alpha, 0, 'f', 3);
}

QIcon toolbarIcon(const QString &kind)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#D9E1E8"), 1.7);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (kind == "split") {
        QRectF rect(7, 8, 18, 16);
        painter.drawRoundedRect(rect, 2, 2);
        painter.drawLine(QPointF(16, 8), QPointF(16, 24));
    } else if (kind == "preview") {
        QRectF rect(7, 8, 18, 16);
        painter.drawRoundedRect(rect, 2, 2);
        painter.fillRect(QRectF(18, 9, 6, 14), QColor("#D9E1E8"));
        painter.drawLine(QPointF(18, 8), QPointF(18, 24));
    } else if (kind == "search") {
        painter.drawEllipse(QRectF(9, 9, 10, 10));
        painter.drawLine(QPointF(17, 17), QPointF(23, 23));
    }

    return QIcon(pixmap);
}

class FolderTreeDelegate : public QStyledItemDelegate {
public:
    explicit FolderTreeDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->icon = QIcon();
        option->decorationSize = QSize(0, 0);
        option->features &= ~QStyleOptionViewItem::HasDecoration;
    }
};
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
      m_verticalSplitter(new QSplitter(Qt::Vertical, this)),
      m_config(AppConfig::loadOrCreate()),
      m_initialPath(initialPath)
{
    setWindowTitle("tfx");
    resize(1280, 780);
    // Translucency must be set before the native surface is created.
    if (m_config.opacity.background < 1.0) {
        setAttribute(Qt::WA_TranslucentBackground);
    }
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
    auto *pinnedLabel = new QLabel(UiText::t("Pinned", "ピン留め"), m_sidebar);
    auto *treeLabel = new QLabel(UiText::t("Folders", "フォルダー"), m_sidebar);
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
    connect(m_terminalPane, &TerminalPane::closeRequested, this, [this]() {
        setTerminalVisible(false);
    });

    buildActions();
    auto *focusOtherPaneShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(focusOtherPaneShortcut, &QShortcut::activated, this, &MainWindow::focusOtherPane);
    auto *focusPreviousPaneShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Tab), this);
    connect(focusPreviousPaneShortcut, &QShortcut::activated, this, &MainWindow::focusOtherPane);
    setActivePane(m_leftPane);
    m_previewPane->previewPath(initialPath);
    restoreSettings();
    QTimer::singleShot(0, this, [this]() {
        activePane()->focusFileList();
    });
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
    addMenuAction(fileMenu, UiText::t("New File", "新規ファイル"), this, [this]() { activePane()->createFile(); }, QKeySequence(m_config.shortcut("newFile", "Ctrl+N")));
    addMenuAction(fileMenu, UiText::t("New Folder", "新規フォルダ"), this, [this]() { activePane()->createFolder(); }, QKeySequence(m_config.shortcut("newFolder", "Ctrl+Shift+N")));
    addMenuAction(fileMenu, UiText::t("Rename", "名前を変更"), this, [this]() { activePane()->renameSelected(); }, QKeySequence(m_config.shortcut("rename", "F2")));
    addMenuAction(fileMenu, UiText::t("Move to Trash", "ゴミ箱へ移動"), this, [this]() { activePane()->moveSelectedToTrash(); }, QKeySequence(m_config.shortcut("moveToTrash", "Del")));
    fileMenu->addSeparator();
    addMenuAction(fileMenu, UiText::t("Quit", "終了"), qApp, []() { QApplication::quit(); }, QKeySequence(m_config.shortcut("quit", "Ctrl+Q")));

    auto *editMenu = menuBar()->addMenu(UiText::t("Edit", "編集"));
    addMenuAction(editMenu, UiText::t("Copy", "コピー"), this, [this]() { activePane()->copySelected(); }, QKeySequence(m_config.shortcut("copyItems", "Ctrl+C")));
    addMenuAction(editMenu, UiText::t("Cut", "カット"), this, [this]() { activePane()->cutSelected(); }, QKeySequence(m_config.shortcut("cutItems", "Ctrl+X")));
    addMenuAction(editMenu, UiText::t("Paste", "ペースト"), this, [this]() { activePane()->pasteIntoCurrentDirectory(); }, QKeySequence(m_config.shortcut("pasteItems", "Ctrl+V")));
    addMenuAction(editMenu, UiText::t("Copy Path", "パスをコピー"), this, [this]() { activePane()->copySelectedPaths(); }, QKeySequence("Ctrl+Shift+C"));

    auto *viewMenu = menuBar()->addMenu(UiText::t("View", "表示"));
    m_splitAction = viewMenu->addAction(UiText::t("Split Pane", "スプリット表示"));
    m_splitAction->setCheckable(true);
    m_splitAction->setChecked(true);
    m_splitAction->setShortcut(QKeySequence(m_config.shortcut("toggleSplit", "Ctrl+\\")));
    connect(m_splitAction, &QAction::toggled, this, &MainWindow::setSplitVisible);

    m_previewAction = viewMenu->addAction(UiText::t("Preview", "プレビュー"));
    m_previewAction->setCheckable(true);
    m_previewAction->setChecked(true);
    m_previewAction->setShortcut(QKeySequence(m_config.shortcut("togglePreview", "Ctrl+Shift+P")));
    connect(m_previewAction, &QAction::toggled, this, &MainWindow::setPreviewVisible);

    m_terminalAction = viewMenu->addAction(UiText::t("Terminal Pane", "ターミナルペイン"));
    m_terminalAction->setCheckable(true);
    m_terminalAction->setShortcut(QKeySequence(m_config.shortcut("toggleTerminal", "Ctrl+J")));
    connect(m_terminalAction, &QAction::toggled, this, &MainWindow::setTerminalVisible);

    m_hiddenAction = viewMenu->addAction(UiText::t("Show Hidden Files", "隠しファイルを表示"));
    m_hiddenAction->setCheckable(true);
    m_hiddenAction->setShortcut(QKeySequence(m_config.shortcut("toggleHidden", "Ctrl+Shift+.")));
    connect(m_hiddenAction, &QAction::toggled, this, &MainWindow::setHiddenFilesVisible);

    addMenuAction(viewMenu, UiText::t("File List Settings...", "ファイル一覧設定..."), this, [this]() {
        activePane()->showColumnSettingsDialog();
    });

    auto *tabMenu = menuBar()->addMenu(UiText::t("Tabs", "タブ"));
    addMenuAction(tabMenu, UiText::t("New Tab", "新規タブ"), this, [this]() { activePane()->newTab(); }, QKeySequence(m_config.shortcut("newTab", "Ctrl+T")));
    addMenuAction(tabMenu, UiText::t("Close Tab", "タブを閉じる"), this, [this]() { activePane()->closeCurrentTab(); }, QKeySequence(m_config.shortcut("closeTab", "Ctrl+W")));
    addMenuAction(tabMenu, UiText::t("Previous Tab", "前のタブ"), this, [this]() { activePane()->previousTab(); }, QKeySequence(m_config.shortcut("prevTab", "Ctrl+Shift+[")));
    addMenuAction(tabMenu, UiText::t("Next Tab", "次のタブ"), this, [this]() { activePane()->nextTab(); }, QKeySequence(m_config.shortcut("nextTab", "Ctrl+Shift+]")));

    auto *navMenu = menuBar()->addMenu(UiText::t("Navigate", "移動"));
    addMenuAction(navMenu, UiText::t("Parent", "親フォルダ"), this, [this]() { activePane()->goUp(); }, QKeySequence(m_config.shortcut("goUp", "Alt+Up")));
    addMenuAction(navMenu, UiText::t("Back", "戻る"), this, [this]() { activePane()->goBack(); }, QKeySequence(m_config.shortcut("goBack", "Alt+Left")));
    addMenuAction(navMenu, UiText::t("Forward", "進む"), this, [this]() { activePane()->goForward(); }, QKeySequence(m_config.shortcut("goForward", "Alt+Right")));
    addMenuAction(navMenu, UiText::t("Reload", "再読み込み"), this, [this]() { activePane()->reload(); }, QKeySequence(m_config.shortcut("reload", "F5")));
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
    auto *focusSearchShortcut = new QShortcut(QKeySequence(m_config.shortcut("focusSearch", "Ctrl+F")), this);
    connect(focusSearchShortcut, &QShortcut::activated, m_searchEdit, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
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
    m_treeView->setItemDelegate(new FolderTreeDelegate(m_treeView));
    m_treeView->setCurrentIndex(m_treeModel->index(initialPath));
    m_treeView->setIndentation(10);
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
    QString styleSheet = R"(
        QMainWindow, QWidget {
            background: #151A1E;
            color: #D9E1E8;
            font-family: __MONO_FONT__;
            font-size: 12px;
        }
        QMenuBar, QMenu {
            background: #11161A;
            color: #D9E1E8;
            border: 1px solid #2A333A;
        }
        QMenu::item { padding: 5px 22px 5px 18px; }
        QMenu::item:selected { background: #243947; color: #FFFFFF; }
        QToolBar#topToolbar {
            background: #151A1E;
            border: 0;
            border-bottom: 1px solid #293137;
            padding: 4px 8px;
            spacing: 5px;
        }
        QLineEdit#pathEdit {
            background: #0F1418;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 0;
            padding: 5px 10px;
            font-weight: 700;
        }
        QLineEdit#searchEdit {
            background: #10161A;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 0;
            padding: 5px 9px;
            selection-background-color: #243947;
        }
        QWidget#sidebar {
            background: #171C20;
            border-right: 1px solid #2A333A;
        }
        QLabel#sectionLabel {
            color: #9EABB6;
            font-weight: 500;
            padding: 7px 0 3px 0;
        }
        QListWidget, QTreeView {
            background: #171C20;
            color: #D9E1E8;
            selection-background-color: #263D4C;
            selection-color: #FFFFFF;
            border: 0;
            outline: 0;
        }
        QTableView#fileTable {
            background: #151A1E;
            color: #D9E1E8;
            selection-background-color: #263D4C;
            selection-color: #FFFFFF;
            border: 0;
            outline: 0;
            gridline-color: #293137;
        }
        QTableView#fileTable::item:selected,
        QTableView#fileTable::item:selected:active,
        QTableView#fileTable::item:selected:!active {
            background-color: #31576B;
            color: #FFFFFF;
        }
        QTableView#fileTable::item:hover {
            background: #1F2830;
        }
        QHeaderView::section {
            background: #10161A;
            color: #B9C4CC;
            border: 0;
            border-top: 1px solid #2A333A;
            border-bottom: 1px solid #2A333A;
            border-right: 1px solid #2A333A;
            padding: 4px 9px;
            font-weight: 500;
        }
        QTabBar#paneTabs {
            background: #151A1E;
            border-bottom: 1px solid #2A333A;
            min-height: 21px;
        }
        QTabBar#paneTabs::tab {
            background: transparent;
            color: #9EABB6;
            border: 1px solid transparent;
            border-radius: 2px;
            padding: 2px 10px;
            margin: 1px 2px 1px 0;
        }
        QTabBar#paneTabs::tab:selected {
            color: #D9E1E8;
            border-color: #364149;
            background: #10161A;
        }
        QWidget#filePane {
            background: #151A1E;
            border: 1px solid #2A333A;
        }
        QWidget#filePane[activePane="true"] {
            border: 2px solid #36E67A;
        }
        QLabel#paneBadge {
            color: #AEBBC5;
            background: transparent;
            border: 1px solid #38434B;
            border-radius: 2px;
            padding: 2px 9px;
            font-weight: 700;
        }
        QLabel#paneBadge[activePane="true"] {
            color: #04140A;
            background: #63F28D;
            border-color: #63F28D;
        }
        QLineEdit#panePath {
            color: #AEBBC5;
            background: transparent;
            border: 0;
            padding: 0;
            font-weight: 600;
            selection-background-color: #263D4C;
        }
        QLineEdit#panePath[activePane="true"] {
            color: #63F28D;
        }
        QLabel#paneStatus {
            background: #10161A;
            color: #B9C4CC;
            border-top: 1px solid #2A333A;
            padding: 5px 10px;
            font-weight: 600;
        }
        QToolButton, QPushButton {
            background: #151B20;
            color: #D9E1E8;
            border: 1px solid #303A42;
            border-radius: 3px;
            padding: 4px 7px;
        }
        QToolButton#toolbarIconButton {
            min-width: 31px;
            max-width: 31px;
            min-height: 24px;
            max-height: 24px;
            padding: 0;
            background: #151B20;
        }
        QToolButton#previewSourceToggle, QToolButton#previewOpenExternal {
            min-width: 32px;
            max-width: 32px;
            min-height: 24px;
            max-height: 24px;
            padding: 0;
            background: #151B20;
        }
        QToolButton:hover, QPushButton:hover {
            background: #1F2830;
            border-color: #4A5963;
        }
        QToolButton#toolbarIconButton:checked,
        QToolButton#previewSourceToggle:checked {
            background: #263D4C;
            border-color: #5C7484;
            color: #FFFFFF;
        }
        QSplitter::handle {
            background: #222A30;
        }
        QSplitter#fileSplitter::handle {
            background: #20272D;
            border-left: 1px solid #2E3941;
            border-right: 1px solid #2E3941;
        }
        QSplitter#mainSplitter::handle,
        QSplitter#verticalSplitter::handle {
            background: #20272D;
            border: 1px solid #2E3941;
        }
        QStatusBar {
            background: #151A1E;
            color: #9EABB6;
            border-top: 1px solid #2A333A;
        }
        QWidget#previewPane {
            background: #151A1E;
            border: 1px solid #2A333A;
        }
        QPlainTextEdit#previewCode, QTextBrowser#previewRendered {
            background: #151A1E;
            color: #D9E1E8;
            border: 0;
            selection-background-color: #263D4C;
        }
        QLabel#previewTitle {
            color: #9EABB6;
            font-weight: 600;
        }
        QWidget#terminalPane {
            background: #050607;
            border-top: 1px solid #2A333A;
        }
        QLabel#terminalTitle {
            color: #9EABB6;
            padding: 4px 6px;
            font-weight: 500;
        }
        QToolButton#terminalCloseButton {
            background: transparent;
            border: 0;
            color: #D9E1E8;
            padding: 0;
            min-width: 20px;
            max-width: 20px;
            min-height: 20px;
            max-height: 20px;
        }
        QToolButton#terminalCloseButton:hover {
            background: #2A333A;
        }
        QPlainTextEdit#terminalOutput {
            background: #050607;
            color: #D9E1E8;
            border: 0;
            selection-background-color: #263D4C;
        }
        QLineEdit#terminalCommand {
            background: #0D1114;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 0;
            padding: 4px 8px;
            selection-background-color: #263D4C;
        }
    )";
    const double bgAlpha = m_config.opacity.background;
    auto surface = [bgAlpha](const QString &color) {
        return bgAlpha < 1.0 ? withAlpha(color, bgAlpha) : color;
    };
    styleSheet.replace("#151A1E", surface(m_config.colors.panelBackground));
    styleSheet.replace("#151B20", surface(m_config.colors.panelBackground));
    styleSheet.replace("#11161A", surface(m_config.colors.appBackground));
    styleSheet.replace("#171C20", surface(m_config.colors.sidebarBackground));
    styleSheet.replace("#10161A", surface(m_config.colors.inputBackground));
    styleSheet.replace("#0F1418", surface(m_config.colors.inputBackground));
    styleSheet.replace("#0D1114", surface(m_config.colors.inputBackground));
    styleSheet.replace("#050607", surface(m_config.colors.terminalBackground));
    styleSheet.replace("#D9E1E8", m_config.colors.foreground);
    styleSheet.replace("#9EABB6", m_config.colors.secondaryForeground);
    styleSheet.replace("#AEBBC5", m_config.colors.secondaryForeground);
    styleSheet.replace("#B9C4CC", m_config.colors.headerForeground);
    styleSheet.replace("#263D4C", m_config.colors.selectedBackground);
    styleSheet.replace("#31576B", m_config.colors.selectedBackground);
    styleSheet.replace("#243947", m_config.colors.selectedBackground);
    styleSheet.replace("#FFFFFF", m_config.colors.selectedForeground);
    styleSheet.replace("#2A333A", m_config.colors.border);
    styleSheet.replace("#293137", m_config.colors.border);
    styleSheet.replace("#303A42", m_config.colors.border);
    styleSheet.replace("#2E3941", m_config.colors.border);
    styleSheet.replace("#36E67A", m_config.colors.activeBorder);
    styleSheet.replace("#63F28D", m_config.colors.activeAccent);
    styleSheet.replace("#1F2830", m_config.colors.hoverBackground);
    styleSheet.replace("__MONO_FONT__", m_config.resolvedMonoFontFamily());
    styleSheet.replace("font-size: 12px;", QString("font-size: %1px;").arg(m_config.font.size));
    if (m_config.opacity.disabledItem < 1.0) {
        styleSheet += QString(
            "\nQWidget:disabled, QToolButton:disabled, QPushButton:disabled,"
            " QMenu::item:disabled, QMenuBar::item:disabled {"
            " color: %1; }\n")
            .arg(withAlpha(m_config.colors.foreground, m_config.opacity.disabledItem));
    }
    qApp->setStyleSheet(styleSheet);
    m_leftPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);
    m_rightPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);
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
    bool splitVisible = settings.value("View/splitVisible", true).toBool();
    bool previewVisible = settings.value("View/previewVisible", true).toBool();
    const bool terminalVisible = settings.value("View/terminalVisible", false).toBool();
    if (m_config.startup.layout == "single") {
        splitVisible = false;
    } else if (m_config.startup.layout == "split") {
        splitVisible = true;
    }
    if (m_config.startup.preview == "show") {
        previewVisible = true;
    } else if (m_config.startup.preview == "hide") {
        previewVisible = false;
    }

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

    QString rightDirectory = settings.value("Panes/rightDirectory", m_rightPane->currentPath()).toString();
    if (!m_config.startup.rightFolder.isEmpty()) {
        rightDirectory = m_config.startup.rightFolder;
    }
    for (const QString &path : m_config.startup.rightFolders) {
        if (QFileInfo(path).isDir()) {
            rightDirectory = path;
            break;
        }
    }
    if (QFileInfo(rightDirectory).isDir()) {
        m_rightPane->navigateTo(rightDirectory, false);
    }
    m_leftPane->restoreTabs(
        settings.value("Tabs/leftPaths").toStringList(),
        settings.value("Tabs/leftActiveIndex", 0).toInt());
    m_rightPane->restoreTabs(
        settings.value("Tabs/rightPaths").toStringList(),
        settings.value("Tabs/rightActiveIndex", 0).toInt());
    if (QFileInfo(m_initialPath).isDir()) {
        m_leftPane->navigateTo(m_initialPath, false);
    }

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
    if (!m_config.warningText().isEmpty()) {
        statusBar()->showMessage(m_config.warningText().section('\n', 0, 0), 6000);
    }
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
    const QList<int> fileSizes = m_fileSplitter->sizes();
    const QList<int> mainSizes = m_mainSplitter->sizes();
    const int sidebarWidth = std::max(m_sidebar->minimumWidth(), mainSizes.value(0, m_sidebar->width()));
    const int fileAreaWidth = std::max(m_leftPane->minimumWidth(), mainSizes.value(1, visibleFileListWidth()));
    const int previewWidth = mainSizes.size() >= 3 ? std::max(0, mainSizes.value(2, 0)) : 0;
    const int leftWidth = visibleFileListWidth();
    const int rightWidth = std::max(0, fileSizes.value(1, 0));

    if (visible && !wasVisible) {
        const int availableWidth = fileAreaWidth;
        const int paneSpace = std::max(0, availableWidth - m_fileSplitter->handleWidth());
        const int targetLeftWidth = std::max(m_leftPane->minimumWidth(), paneSpace / 2);
        const int targetRightWidth = std::max(m_rightPane->minimumWidth(), paneSpace - targetLeftWidth);
        // The right pane mirrors the left pane's shared column layout when shown.
        m_rightPane->applySharedColumnLayout();
        m_rightPane->setVisible(true);
        syncLayoutConstraints();
        m_mainSplitter->setSizes(normalizedMainSplitterSizes(sidebarWidth, availableWidth, previewWidth));
        m_fileSplitter->setSizes(normalizedFileSplitterSizes(targetLeftWidth, targetRightWidth));
        m_lastSplitWidth = targetRightWidth;
    } else if (!visible && wasVisible) {
        if (rightWidth > 80) {
            m_lastSplitWidth = rightWidth;
        }
        const int targetLeftWidth = std::max(m_leftPane->minimumWidth(), leftWidth + m_fileSplitter->handleWidth() + rightWidth);
        m_rightPane->setVisible(false);
        syncLayoutConstraints();
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

    m_searchEdit->clear();
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

FilePane *MainWindow::activePane() const
{
    return m_activePane ? m_activePane : m_leftPane;
}
