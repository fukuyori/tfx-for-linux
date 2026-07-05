#include "MainWindow.h"
#include "UiText.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPolygonF>
#include <QShortcut>
#include <QSizePolicy>
#include <QToolButton>

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
    QPen pen(QColor("#D9E1E8"), 1.7);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (kind == "back") {
        QPen arrow(QColor("#D9E1E8"), 2);
        arrow.setCapStyle(Qt::RoundCap);
        arrow.setJoinStyle(Qt::RoundJoin);
        painter.setPen(arrow);
        painter.drawPolyline(QPolygonF({QPointF(19, 9), QPointF(12, 16), QPointF(19, 23)}));
    } else if (kind == "forward") {
        QPen arrow(QColor("#D9E1E8"), 2);
        arrow.setCapStyle(Qt::RoundCap);
        arrow.setJoinStyle(Qt::RoundJoin);
        painter.setPen(arrow);
        painter.drawPolyline(QPolygonF({QPointF(13, 9), QPointF(20, 16), QPointF(13, 23)}));
    } else if (kind == "up") {
        QPen arrow(QColor("#D9E1E8"), 2);
        arrow.setCapStyle(Qt::RoundCap);
        arrow.setJoinStyle(Qt::RoundJoin);
        painter.setPen(arrow);
        painter.drawLine(QPointF(16, 8), QPointF(16, 24));
        painter.drawPolyline(QPolygonF({QPointF(9, 15), QPointF(16, 8), QPointF(23, 15)}));
    } else if (kind == "hidden") {
        painter.drawEllipse(QRectF(9, 12, 14, 8));
        painter.setBrush(QColor("#D9E1E8"));
        painter.drawEllipse(QRectF(13.5, 13.5, 5, 5));
        painter.setBrush(Qt::NoBrush);
    } else if (kind == "sidebar") {
        QRectF rect(7, 8, 18, 16);
        painter.drawRoundedRect(rect, 2, 2);
        painter.fillRect(QRectF(8, 9, 6, 14), QColor("#D9E1E8"));
        painter.drawLine(QPointF(14, 8), QPointF(14, 24));
    } else if (kind == "split") {
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
    } else if (kind == "icons") {
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                painter.drawRoundedRect(QRectF(8 + col * 9, 8 + row * 9, 7, 7), 1.5, 1.5);
            }
        }
    } else if (kind == "output") {
        painter.drawRoundedRect(QRectF(6, 8, 20, 16), 2, 2);
        QPen line(QColor("#D9E1E8"), 1.6);
        line.setCapStyle(Qt::RoundCap);
        painter.setPen(line);
        painter.drawLine(QPointF(10, 13), QPointF(22, 13));
        painter.drawLine(QPointF(10, 16), QPointF(19, 16));
        painter.drawLine(QPointF(10, 19), QPointF(22, 19));
    } else if (kind == "terminal") {
        painter.drawRoundedRect(QRectF(6, 8, 20, 16), 2, 2);
        QPen prompt(QColor("#63F28D"), 2);
        prompt.setCapStyle(Qt::RoundCap);
        prompt.setJoinStyle(Qt::RoundJoin);
        painter.setPen(prompt);
        painter.drawPolyline(QPolygonF({QPointF(10, 13), QPointF(14, 16), QPointF(10, 19)}));
        painter.drawLine(QPointF(16, 19), QPointF(22, 19));
    }

    return QIcon(pixmap);
}
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
    m_sidebarAction = viewMenu->addAction(UiText::t("Folder Sidebar", "フォルダーサイドバー"));
    m_sidebarAction->setCheckable(true);
    m_sidebarAction->setChecked(true);
    connect(m_sidebarAction, &QAction::toggled, this, &MainWindow::setSidebarVisible);

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

    m_commandOutputAction = viewMenu->addAction(UiText::t("Command Output", "コマンド出力"));
    m_commandOutputAction->setCheckable(true);
    connect(m_commandOutputAction, &QAction::toggled, this, &MainWindow::setCommandOutputVisible);

    m_hiddenAction = viewMenu->addAction(UiText::t("Show Hidden Files", "隠しファイルを表示"));
    m_hiddenAction->setCheckable(true);
    m_hiddenAction->setShortcut(QKeySequence(m_config.shortcut("toggleHidden", "Ctrl+Shift+.")));
    connect(m_hiddenAction, &QAction::toggled, this, &MainWindow::setHiddenFilesVisible);

    m_iconViewAction = viewMenu->addAction(UiText::t("Icon View", "アイコン表示"));
    m_iconViewAction->setCheckable(true);
    connect(m_iconViewAction, &QAction::toggled, this, &MainWindow::setIconViewEnabled);

    addMenuAction(viewMenu, UiText::t("File List Settings...", "ファイル一覧設定..."), this, [this]() {
        activePane()->showColumnSettingsDialog();
    });

    viewMenu->addSeparator();
    addMenuAction(viewMenu, UiText::t("Reset Layout", "レイアウトを初期化"), this,
                  [this]() { resetDockLayout(); });

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

    bool hasCommandMenu = false;
    QMenu *commandMenu = nullptr;
    for (int i = 0; i < m_config.commands.size(); ++i) {
        const UserCommand &command = m_config.commands.at(i);
        if (command.name.trimmed().isEmpty() || command.run.trimmed().isEmpty()) {
            continue;
        }
        if (!hasCommandMenu) {
            commandMenu = menuBar()->addMenu(UiText::t("Commands", "コマンド"));
            hasCommandMenu = true;
        }
        addMenuAction(commandMenu, command.name, this, [this, i]() {
            activePane()->runUserCommand(i);
        }, QKeySequence(command.shortcut));
    }
}

void MainWindow::buildTopToolbar()
{
    m_topToolbar->setObjectName("topToolbar");
    m_topToolbar->setMovable(false);
    m_topToolbar->setFloatable(false);
    m_topToolbar->setIconSize(QSize(18, 18));
    addToolBar(Qt::TopToolBarArea, m_topToolbar);

    auto *backButton = new QToolButton(this);
    backButton->setObjectName("toolbarIconButton");
    backButton->setIcon(toolbarIcon("back"));
    backButton->setIconSize(QSize(24, 24));
    backButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    backButton->setToolTip(UiText::t("Back", "戻る"));
    connect(backButton, &QToolButton::clicked, this, [this]() { activePane()->goBack(); });
    m_topToolbar->addWidget(backButton);

    auto *forwardButton = new QToolButton(this);
    forwardButton->setObjectName("toolbarIconButton");
    forwardButton->setIcon(toolbarIcon("forward"));
    forwardButton->setIconSize(QSize(24, 24));
    forwardButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    forwardButton->setToolTip(UiText::t("Forward", "進む"));
    connect(forwardButton, &QToolButton::clicked, this, [this]() { activePane()->goForward(); });
    m_topToolbar->addWidget(forwardButton);

    auto *upButton = new QToolButton(this);
    upButton->setObjectName("toolbarIconButton");
    upButton->setIcon(toolbarIcon("up"));
    upButton->setIconSize(QSize(24, 24));
    upButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    upButton->setToolTip(UiText::t("Parent Folder", "上の階層へ"));
    connect(upButton, &QToolButton::clicked, this, [this]() { activePane()->goUp(); });
    m_topToolbar->addWidget(upButton);

    auto *navSpacer = new QWidget(this);
    navSpacer->setFixedWidth(16);
    m_topToolbar->addWidget(navSpacer);

    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setEditable(true);
    m_searchEdit->setPlaceholderText(UiText::t("Search (Enter to search subfolders)", "検索 (Enter でサブフォルダ検索)"));
    m_searchEdit->lineEdit()->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(260);
    m_searchEdit->setInsertPolicy(QComboBox::NoInsert);
    connect(m_searchEdit->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::runSearchFromToolbar);
    connect(m_searchEdit, &QComboBox::activated, this, [this](int) {
        runSearchFromToolbar();
    });
    auto *focusSearchShortcut = new QShortcut(QKeySequence(m_config.shortcut("focusSearch", "Ctrl+F")), this);
    connect(focusSearchShortcut, &QShortcut::activated, m_searchEdit, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->lineEdit()->selectAll();
    });
    m_topToolbar->addWidget(m_searchEdit);
    auto *cancelSearchButton = new QToolButton(this);
    cancelSearchButton->setObjectName("toolbarIconButton");
    cancelSearchButton->setText("x");
    cancelSearchButton->setFixedSize(24, 24);
    cancelSearchButton->setToolTip(UiText::t("Close search results", "検索結果を閉じる"));
    connect(cancelSearchButton, &QToolButton::clicked, this, [this]() {
        activePane()->cancelSearch();
        activePane()->focusFileList();
        m_searchEdit->clearEditText();
    });
    m_topToolbar->addWidget(cancelSearchButton);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_topToolbar->addWidget(spacer);

    m_hiddenButton = new QToolButton(this);
    m_hiddenButton->setObjectName("toolbarIconButton");
    m_hiddenButton->setIcon(toolbarIcon("hidden"));
    m_hiddenButton->setIconSize(QSize(24, 24));
    m_hiddenButton->setCheckable(true);
    m_hiddenButton->setChecked(m_showHiddenFiles);
    m_hiddenButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_hiddenButton->setToolTip(UiText::t("Show / hide hidden files", "不可視ファイルの表示切り替え"));
    connect(m_hiddenButton, &QToolButton::toggled, this, &MainWindow::setHiddenFilesVisible);
    m_topToolbar->addWidget(m_hiddenButton);

    m_sidebarButton = new QToolButton(this);
    m_sidebarButton->setObjectName("toolbarIconButton");
    m_sidebarButton->setIcon(toolbarIcon("sidebar"));
    m_sidebarButton->setIconSize(QSize(24, 24));
    m_sidebarButton->setCheckable(true);
    m_sidebarButton->setChecked(true);
    m_sidebarButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_sidebarButton->setToolTip(UiText::t("Show / hide folder sidebar", "フォルダーペインの表示切り替え"));
    connect(m_sidebarButton, &QToolButton::toggled, this, &MainWindow::setSidebarVisible);
    m_topToolbar->addWidget(m_sidebarButton);

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

    m_iconViewButton = new QToolButton(this);
    m_iconViewButton->setObjectName("toolbarIconButton");
    m_iconViewButton->setIcon(toolbarIcon("icons"));
    m_iconViewButton->setIconSize(QSize(24, 24));
    m_iconViewButton->setCheckable(true);
    m_iconViewButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_iconViewButton->setToolTip(UiText::t("Details / icon view", "詳細表示 / アイコン表示"));
    connect(m_iconViewButton, &QToolButton::toggled, this, &MainWindow::setIconViewEnabled);
    m_topToolbar->addWidget(m_iconViewButton);

    m_terminalButton = new QToolButton(this);
    m_terminalButton->setObjectName("toolbarIconButton");
    m_terminalButton->setIcon(toolbarIcon("terminal"));
    m_terminalButton->setIconSize(QSize(24, 24));
    m_terminalButton->setCheckable(true);
    m_terminalButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_terminalButton->setToolTip(UiText::t("Show / hide terminal", "ターミナルを表示 / 非表示"));
    connect(m_terminalButton, &QToolButton::toggled, this, &MainWindow::setTerminalVisible);
    m_topToolbar->addWidget(m_terminalButton);

    m_commandOutputButton = new QToolButton(this);
    m_commandOutputButton->setObjectName("toolbarIconButton");
    m_commandOutputButton->setIcon(toolbarIcon("output"));
    m_commandOutputButton->setIconSize(QSize(24, 24));
    m_commandOutputButton->setCheckable(true);
    m_commandOutputButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_commandOutputButton->setToolTip(UiText::t("Show / hide command output", "コマンド出力を表示 / 非表示"));
    connect(m_commandOutputButton, &QToolButton::toggled, this, &MainWindow::setCommandOutputVisible);
    m_topToolbar->addWidget(m_commandOutputButton);
}
