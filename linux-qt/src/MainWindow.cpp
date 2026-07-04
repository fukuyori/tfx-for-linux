#include "MainWindow.h"
#include "UiText.h"
#include "platform/Platform.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsOpacityEffect>
#include <QListWidget>
#include <QMimeData>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QFont>
#include <QPainter>
#include <QPolygonF>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
struct ParsedGeometry
{
    QSize size;
    QPoint position;
    bool hasPosition = false;
};

bool parseWindowGeometry(const QString &text, ParsedGeometry *geometry)
{
    static const QRegularExpression pattern(R"(^\s*(\d+)x(\d+)([+-]\d+)?([+-]\d+)?\s*$)");
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    bool widthOk = false;
    bool heightOk = false;
    const int width = match.captured(1).toInt(&widthOk);
    const int height = match.captured(2).toInt(&heightOk);
    if (!widthOk || !heightOk || width < 400 || height < 300) {
        return false;
    }

    geometry->size = QSize(width, height);
    if (!match.captured(3).isEmpty() && !match.captured(4).isEmpty()) {
        geometry->position = QPoint(match.captured(3).toInt(), match.captured(4).toInt());
        geometry->hasPosition = true;
    }
    return true;
}

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
    } else if (kind == "icons") {
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                painter.drawRoundedRect(QRectF(8 + col * 9, 8 + row * 9, 7, 7), 1.5, 1.5);
            }
        }
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

// Pinned-folder list: supports reordering by drag, accepts folders dropped from
// the file list, and draws an insertion indicator at the drop position.
class PinnedListWidget : public QListWidget {
public:
    using QListWidget::QListWidget;

    std::function<void(const QStringList &folders, int row)> onExternalFoldersDropped;
    std::function<void()> onReordered;

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        Q_UNUSED(supportedActions);
        // Custom drag: the item is repositioned in dropEvent, so the base view's
        // post-move row removal must be avoided (it would delete the item).
        if (!currentItem()) {
            return;
        }
        auto *drag = new QDrag(this);
        drag->setMimeData(new QMimeData());
        drag->exec(Qt::MoveAction);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == this || event->mimeData()->hasUrls()) {
            m_dragActive = true;
            m_dropRow = dropRowAt(event->position().toPoint());
            event->acceptProposedAction();
            viewport()->update();
            return;
        }
        QListWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->source() == this || event->mimeData()->hasUrls()) {
            m_dragActive = true;
            m_dropRow = dropRowAt(event->position().toPoint());
            event->acceptProposedAction();
            viewport()->update();
            return;
        }
        QListWidget::dragMoveEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        m_dragActive = false;
        viewport()->update();
        QListWidget::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        const int row = dropRowAt(event->position().toPoint());
        m_dragActive = false;
        viewport()->update();

        if (event->source() == this) {
            const int from = currentRow();
            if (from >= 0) {
                int to = row;
                if (to > from) {
                    to -= 1;
                }
                QListWidgetItem *moved = takeItem(from);
                insertItem(qBound(0, to, count()), moved);
                setCurrentItem(moved);
                if (onReordered) {
                    onReordered();
                }
            }
            event->acceptProposedAction();
            return;
        }

        if (event->mimeData()->hasUrls()) {
            QStringList folders;
            for (const QUrl &url : event->mimeData()->urls()) {
                const QString path = url.toLocalFile();
                if (!path.isEmpty()) {
                    folders << path;
                }
            }
            if (onExternalFoldersDropped) {
                onExternalFoldersDropped(folders, row);
            }
            event->acceptProposedAction();
            return;
        }
        QListWidget::dropEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QListWidget::paintEvent(event);
        if (!m_dragActive) {
            return;
        }
        int y = 0;
        if (m_dropRow <= 0) {
            y = count() > 0 ? visualItemRect(item(0)).top() : 0;
        } else if (m_dropRow >= count()) {
            y = count() > 0 ? visualItemRect(item(count() - 1)).bottom() : 0;
        } else {
            y = visualItemRect(item(m_dropRow)).top();
        }
        QPainter painter(viewport());
        painter.setPen(QPen(QColor("#63F28D"), 2));
        painter.drawLine(2, y, viewport()->width() - 2, y);
    }

private:
    int dropRowAt(const QPoint &pos)
    {
        const QModelIndex index = indexAt(pos);
        if (!index.isValid()) {
            return count();
        }
        const QRect rect = visualItemRect(item(index.row()));
        return pos.y() > rect.center().y() ? index.row() + 1 : index.row();
    }

    bool m_dragActive = false;
    int m_dropRow = 0;
};
}

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
    // The left/right file panes are identified by position, so their dock
    // titles are left blank.
    m_dockLeftPane = makeDock("dockLeftPane", QString(), m_leftPane);
    m_dockRightPane = makeDock("dockRightPane", QString(), m_rightPane);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_fileOperationWorker) {
        const int pendingCount = queuedFileOperationCount();
        const QString prompt = pendingCount > 0
            ? UiText::t("A file operation is still running and %1 item(s) are queued. Cancel them and quit?",
                        "ファイル操作を実行中で、%1 件が待機中です。キャンセルして終了しますか？").arg(pendingCount)
            : UiText::t("A file operation is still running. Cancel it and quit?",
                        "ファイル操作を実行中です。キャンセルして終了しますか？");
        const auto result = QMessageBox::question(
            this,
            UiText::t("File Operation Running", "ファイル操作を実行中"),
            prompt);
        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_queuedFileOperations.clear();
        m_closeAfterFileOperationCancel = true;
        cancelFileOperation();
        event->ignore();
        return;
    }
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::startFileOperation(const QVector<FileOperationRequest> &requests)
{
    if (requests.isEmpty()) {
        return;
    }
    if (m_fileOperationWorker) {
        m_queuedFileOperations += requests;
        updateFileOperationSummary();
        statusBar()->showMessage(
            UiText::t("File operation queued (%1 pending item(s)).", "ファイル操作をキューに追加しました（%1 件待機中）。")
                .arg(queuedFileOperationCount()),
            3000);
        return;
    }

    m_fileOperationThread = new QThread(this);
    m_fileOperationWorker = new FileOperationWorker(requests);
    m_fileOperationWorker->moveToThread(m_fileOperationThread);
    m_lastFileOperationCompleted = 0;
    m_lastFileOperationTotal = 0;

    m_fileOperationProgress->setRange(0, 0);
    m_fileOperationProgress->setValue(0);
    m_fileOperationSummary->show();
    m_fileOperationProgress->show();
    m_fileOperationCancel->setEnabled(true);
    m_fileOperationCancel->show();
    updateFileOperationSummary(0, 0);
    statusBar()->showMessage(UiText::t("File operation running...", "ファイル操作を実行中..."));

    connect(m_fileOperationThread, &QThread::started, m_fileOperationWorker, &FileOperationWorker::run);
    connect(m_fileOperationThread, &QThread::finished, m_fileOperationThread, &QObject::deleteLater);
    connect(m_fileOperationWorker, &FileOperationWorker::prepared, this,
            [this](int total) {
        m_fileOperationProgress->setRange(0, qMax(1, total));
        m_fileOperationProgress->setValue(0);
        updateFileOperationSummary(0, total);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::progress, this,
            [this](int completed, int total, const QString &path) {
        m_fileOperationProgress->setRange(0, qMax(1, total));
        m_fileOperationProgress->setValue(qMin(completed, total));
        updateFileOperationSummary(completed, total);
        statusBar()->showMessage(UiText::t("Transferring: %1", "転送中: %1").arg(QFileInfo(path).fileName()));
    });
    connect(m_fileOperationWorker, &FileOperationWorker::finished, this,
            [this](const QStringList &directories) {
        completeFileOperation(directories, UiText::t("File operation completed.", "ファイル操作が完了しました。"), true);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::failed, this,
            [this](const QString &message, const QStringList &directories) {
        completeFileOperation(directories, message, false);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::canceled, this,
            [this](const QStringList &directories) {
        completeFileOperation(directories, UiText::t("File operation canceled.", "ファイル操作をキャンセルしました。"), false);
    });

    m_fileOperationThread->start();
}

void MainWindow::cancelFileOperation()
{
    if (!m_fileOperationWorker) {
        return;
    }
    m_queuedFileOperations.clear();
    updateFileOperationSummary();
    m_fileOperationCancel->setEnabled(false);
    statusBar()->showMessage(UiText::t("Canceling file operation and clearing the queue...",
                                      "ファイル操作をキャンセルし、キューをクリアしています..."));
    m_fileOperationWorker->cancel();
}

void MainWindow::completeFileOperation(const QStringList &directories, const QString &message, bool continueQueued)
{
    reloadChangedDirectories(directories);

    const int pendingCount = queuedFileOperationCount();
    QString finalMessage = message;
    if (!continueQueued && pendingCount > 0) {
        m_queuedFileOperations.clear();
        finalMessage += UiText::t(" Queued item(s) cleared: %1.", " 待機中の項目をクリアしました: %1 件。").arg(pendingCount);
    }

    FileOperationWorker *worker = m_fileOperationWorker;
    QThread *thread = m_fileOperationThread;
    const bool closeAfterCancel = m_closeAfterFileOperationCancel;
    m_fileOperationWorker = nullptr;
    m_fileOperationThread = nullptr;
    m_closeAfterFileOperationCancel = false;

    if (thread && closeAfterCancel) {
        connect(thread, &QThread::finished, this, [this]() {
            QTimer::singleShot(0, this, &QWidget::close);
        });
    }
    if (worker) {
        worker->deleteLater();
    }
    if (thread) {
        thread->quit();
    }

    m_fileOperationProgress->hide();
    m_fileOperationSummary->hide();
    m_fileOperationCancel->hide();
    m_lastFileOperationCompleted = 0;
    m_lastFileOperationTotal = 0;
    statusBar()->showMessage(finalMessage, 5000);

    if (closeAfterCancel) {
        if (!thread) {
            QTimer::singleShot(0, this, &QWidget::close);
        }
        return;
    }

    if (continueQueued && !m_queuedFileOperations.isEmpty()) {
        const QVector<FileOperationRequest> queued = m_queuedFileOperations;
        m_queuedFileOperations.clear();
        QTimer::singleShot(0, this, [this, queued]() {
            startFileOperation(queued);
        });
    }
}

void MainWindow::reloadChangedDirectories(const QStringList &directories)
{
    for (FilePane *candidate : {m_leftPane, m_rightPane}) {
        if (directories.contains(candidate->currentPath())) {
            candidate->reload();
        }
    }
}

int MainWindow::queuedFileOperationCount() const
{
    return m_queuedFileOperations.size();
}

void MainWindow::updateFileOperationSummary(int completed, int total)
{
    if (!m_fileOperationSummary || !m_fileOperationProgress) {
        return;
    }

    if (total >= 0) {
        m_lastFileOperationCompleted = qMax(0, completed);
        m_lastFileOperationTotal = qMax(0, total);
    }

    QString text = m_lastFileOperationTotal > 0
        ? UiText::t("Files %1/%2", "ファイル %1/%2").arg(m_lastFileOperationCompleted).arg(m_lastFileOperationTotal)
        : UiText::t("Preparing", "準備中");

    const int queuedCount = queuedFileOperationCount();
    if (queuedCount > 0) {
        text += UiText::t(" | queued %1", " | 待機 %1").arg(queuedCount);
    }
    m_fileOperationSummary->setText(text);
    m_fileOperationProgress->setFormat(m_lastFileOperationTotal > 0
        ? QString("%1/%2").arg(m_lastFileOperationCompleted).arg(m_lastFileOperationTotal)
        : QString());
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
        if (command.name.trimmed().isEmpty() || command.command.trimmed().isEmpty()) {
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
}

void MainWindow::buildFolderSidebar(const QString &initialPath)
{
    m_treeModel->setRootPath("/");
    m_treeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_treeView->setObjectName("folderTree");
    m_pinnedList->setObjectName("pinnedList");
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
    // The folder tree never participates in drag-and-drop: its folders cannot be
    // dragged out, and items cannot be dropped onto it.
    m_treeView->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_treeView->setDragEnabled(false);
    m_treeView->setAcceptDrops(false);

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
            tfx::platform::revealInFileManager(path);
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Collapse All", "すべて折りたたむ"), this, &MainWindow::collapseFolderTree);
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, path]() {
            addPinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->openAt(path);
            setTerminalVisible(true);
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
    // Reorder pinned folders by dragging, and accept folders dropped from the
    // file list; an insertion indicator shows where the item will land.
    m_pinnedList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pinnedList->setDragEnabled(true);
    m_pinnedList->setAcceptDrops(true);
    m_pinnedList->viewport()->setAcceptDrops(true);
    m_pinnedList->setDropIndicatorShown(false);
    m_pinnedList->setDragDropMode(QAbstractItemView::DragDrop);
    m_pinnedList->setDefaultDropAction(Qt::MoveAction);
    auto *pinned = static_cast<PinnedListWidget *>(m_pinnedList);
    pinned->onReordered = [this]() {
        updatePinnedFolderArea();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    };
    pinned->onExternalFoldersDropped = [this](const QStringList &folders, int row) {
        int insertAt = qBound(0, row, m_pinnedList->count());
        for (const QString &path : folders) {
            const QFileInfo info(path);
            if (!info.isDir()) {
                continue;
            }
            const QString clean = info.canonicalFilePath().isEmpty()
                ? info.absoluteFilePath() : info.canonicalFilePath();
            bool exists = false;
            for (int r = 0; r < m_pinnedList->count(); ++r) {
                if (m_pinnedList->item(r)->data(Qt::UserRole).toString() == clean) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                continue;
            }
            auto *item = new QListWidgetItem(info.fileName().isEmpty() ? clean : info.fileName());
            item->setSizeHint(QSize(0, 18));
            item->setData(Qt::UserRole, clean);
            m_pinnedList->insertItem(insertAt, item);
            ++insertAt;
        }
        updatePinnedFolderArea();
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    };
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
            tfx::platform::revealInFileManager(path);
        });
        menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addSeparator();
        menu.addAction(UiText::t("Unpin Folder", "ピン留めを解除"), this, [this, path]() {
            removePinnedFolder(path);
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this, path]() {
            m_terminalPane->openAt(path);
            setTerminalVisible(true);
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
        QComboBox#searchEdit,
        QComboBox#searchEdit QLineEdit {
            background: #10161A;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 0;
            padding: 5px 9px;
            selection-background-color: #243947;
        }
        QComboBox#searchEdit::drop-down {
            border: 0;
            width: 18px;
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
        QMainWindow::separator {
            background: #222A30;
            width: 7px;
            height: 7px;
        }
        QMainWindow::separator:hover {
            background: #5C7484;
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

    // Per-pane font overrides (empty family / 0 size inherits the global mono).
    const auto paneFontRule = [this](const QString &selector, const QString &family, int size) {
        const QString resolvedFamily = family.isEmpty() ? m_config.resolvedMonoFontFamily() : family;
        const int resolvedSize = size > 0 ? size : m_config.font.size;
        return QString("\n%1 { font-family: %2; font-size: %3px; }\n")
            .arg(selector, resolvedFamily).arg(resolvedSize);
    };
    styleSheet += paneFontRule("QTableView#fileTable, QListView#fileIcons",
                               m_config.font.fileListFamily, m_config.font.fileListSize);
    styleSheet += paneFontRule("QPlainTextEdit#previewCode, QTextBrowser#previewRendered",
                               m_config.font.previewFamily, m_config.font.previewSize);
    styleSheet += paneFontRule("QTreeView#folderTree, QListWidget#pinnedList",
                               m_config.font.folderTreeFamily, m_config.font.folderTreeSize);

    qApp->setStyleSheet(styleSheet);

    // The terminal renders its own text, so its font is set directly.
    m_terminalPane->setContentFont(TerminalPane::resolveFont(
        m_config.font.terminalFamily,
        m_config.font.terminalSize > 0 ? m_config.font.terminalSize : m_config.font.size));
    m_terminalPane->setColorScheme(m_config.terminalColorScheme);
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
    bool sidebarVisible = settings.value("View/sidebarVisible", true).toBool();
    bool splitVisible = settings.value("View/splitVisible", true).toBool();
    bool previewVisible = settings.value("View/previewVisible", true).toBool();
    bool terminalVisible = settings.value("View/terminalVisible", false).toBool();
    const bool commandOutputVisible = settings.value("View/commandOutputVisible", false).toBool();
    m_leftPane->setViewMode(settings.value("View/leftIconMode", false).toBool());
    m_rightPane->setViewMode(settings.value("View/rightIconMode", false).toBool());
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
    if (m_config.startup.terminal == "show") {
        terminalVisible = true;
    } else if (m_config.startup.terminal == "hide") {
        terminalVisible = false;
    }
    if (m_config.startup.folderTree == "show") {
        sidebarVisible = true;
    } else if (m_config.startup.folderTree == "hide") {
        sidebarVisible = false;
    }

    const QString requestedGeometry = !m_geometryOverride.isEmpty()
        ? m_geometryOverride
        : m_config.startup.geometry;
    if (!requestedGeometry.isEmpty()) {
        ParsedGeometry geometry;
        if (parseWindowGeometry(requestedGeometry, &geometry)) {
            if (geometry.hasPosition) {
                setGeometry(QRect(geometry.position, geometry.size));
            } else {
                resize(geometry.size);
            }
        } else {
            statusBar()->showMessage(UiText::t("Invalid geometry: %1", "無効なジオメトリ: %1").arg(requestedGeometry), 6000);
        }
    } else {
        const QByteArray geometry = settings.value("MainWindow/geometry").toByteArray();
        if (!geometry.isEmpty()) {
            restoreGeometry(geometry);
        }
    }

    // Restore the saved dock layout, then apply per-pane visibility on top.
    // Version 1: terminal moved into the vertical splitter (was a bottom dock
    // area). A mismatching/older state is ignored, keeping the layout built above.
    const QByteArray state = settings.value("MainWindow/state").toByteArray();
    if (!state.isEmpty()) {
        restoreState(state, 1);
    }

    m_dockSidebar->setVisible(sidebarVisible);
    m_dockLeftPane->setVisible(true);
    m_dockRightPane->setVisible(splitVisible);
    m_dockPreview->setVisible(previewVisible);
    m_dockTerminal->setVisible(terminalVisible);
    m_dockCommandOutput->setVisible(commandOutputVisible);

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
    const QStringList searchHistory = settings.value("Search/history").toStringList();
    m_searchEdit->clear();
    for (const QString &term : searchHistory) {
        if (!term.trimmed().isEmpty()) {
            m_searchEdit->addItem(term.trimmed());
        }
    }
    m_searchEdit->clearEditText();

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
    if (m_terminalButton) {
        const QSignalBlocker terminalButtonBlocker(m_terminalButton);
        m_terminalButton->setChecked(terminalVisible);
    }
    if (m_sidebarAction) {
        const QSignalBlocker sidebarActionBlocker(m_sidebarAction);
        m_sidebarAction->setChecked(sidebarVisible);
    }
    if (m_commandOutputAction) {
        const QSignalBlocker outputActionBlocker(m_commandOutputAction);
        m_commandOutputAction->setChecked(commandOutputVisible);
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
    // saveState() captures the full dock layout (positions, sizes, floating)
    // and the toolbar, replacing the old per-splitter persistence.
    settings.setValue("MainWindow/state", saveState(1));
    settings.setValue("View/sidebarVisible", m_dockSidebar->isVisible());
    settings.setValue("View/splitVisible", m_dockRightPane->isVisible());
    settings.setValue("View/previewVisible", m_dockPreview->isVisible());
    settings.setValue("View/terminalVisible", m_dockTerminal->isVisible());
    settings.setValue("View/commandOutputVisible", m_dockCommandOutput->isVisible());
    settings.setValue("View/showHiddenFiles", m_showHiddenFiles);
    settings.setValue("View/leftIconMode", m_leftPane->isIconMode());
    settings.setValue("View/rightIconMode", m_rightPane->isIconMode());
    settings.setValue("Panes/activePane", activePane() == m_rightPane ? "RIGHT" : "LEFT");
    settings.setValue("Panes/leftDirectory", m_leftPane->currentPath());
    settings.setValue("Panes/rightDirectory", m_rightPane->currentPath());
    settings.setValue("Tabs/leftPaths", m_leftPane->tabPaths());
    settings.setValue("Tabs/rightPaths", m_rightPane->tabPaths());
    settings.setValue("Tabs/leftActiveIndex", m_leftPane->activeTabIndex());
    settings.setValue("Tabs/rightActiveIndex", m_rightPane->activeTabIndex());
    QStringList searchHistory;
    for (int index = 0; index < m_searchEdit->count(); ++index) {
        searchHistory << m_searchEdit->itemText(index);
    }
    settings.setValue("Search/history", searchHistory);

    QStringList pinnedPaths;
    for (int row = 0; row < m_pinnedList->count(); ++row) {
        pinnedPaths.append(m_pinnedList->item(row)->data(Qt::UserRole).toString());
    }
    settings.setValue("Sidebar/pinnedFolders", pinnedPaths);
}

void MainWindow::runSearchFromToolbar()
{
    const QString term = m_searchEdit->currentText().trimmed();
    if (term.isEmpty()) {
        return;
    }
    rememberSearchTerm(term);
    activePane()->startSearch(term);
}

void MainWindow::rememberSearchTerm(const QString &term)
{
    const QString trimmed = term.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    for (int index = m_searchEdit->count() - 1; index >= 0; --index) {
        if (m_searchEdit->itemText(index).compare(trimmed, Qt::CaseInsensitive) == 0) {
            m_searchEdit->removeItem(index);
        }
    }
    m_searchEdit->insertItem(0, trimmed);
    while (m_searchEdit->count() > 10) {
        m_searchEdit->removeItem(m_searchEdit->count() - 1);
    }
    m_searchEdit->setCurrentIndex(0);
    m_searchEdit->setEditText(trimmed);
    saveSettings();
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

void MainWindow::collapseFolderTree()
{
    m_treeView->collapseAll();
    const QModelIndex current = m_treeModel->index(activePane()->currentPath());
    if (current.isValid()) {
        m_treeView->setCurrentIndex(current);
        m_treeView->scrollTo(current, QAbstractItemView::PositionAtCenter);
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

QDockWidget *MainWindow::makeDock(const QString &objectName, const QString &title, QWidget *content)
{
    auto *dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setWidget(content);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    return dock;
}

void MainWindow::applyDefaultDockLayout()
{
    // Re-dock everything (in case any pane is floating) and rebuild the default
    // arrangement: a vertical splitter with the file panes across the top row
    // and the terminal spanning the full width beneath them. Splitting the
    // terminal in first makes it the bottom child of the vertical splitter, so
    // it sits below BOTH file panes with a draggable separator.
    for (QDockWidget *dock : {m_dockSidebar, m_dockLeftPane, m_dockRightPane,
                              m_dockPreview, m_dockTerminal, m_dockCommandOutput}) {
        dock->setFloating(false);
    }
    addDockWidget(Qt::LeftDockWidgetArea, m_dockSidebar);
    splitDockWidget(m_dockSidebar, m_dockTerminal, Qt::Vertical);
    splitDockWidget(m_dockTerminal, m_dockCommandOutput, Qt::Horizontal);
    splitDockWidget(m_dockSidebar, m_dockLeftPane, Qt::Horizontal);
    splitDockWidget(m_dockLeftPane, m_dockRightPane, Qt::Horizontal);
    splitDockWidget(m_dockRightPane, m_dockPreview, Qt::Horizontal);
    resizeDocks({m_dockSidebar, m_dockLeftPane, m_dockRightPane, m_dockPreview},
                {200, 460, 460, 360}, Qt::Horizontal);
}

void MainWindow::resetDockLayout()
{
    // Preserve which panes are currently shown; only their arrangement resets.
    const bool rightVisible = m_dockRightPane->isVisible();
    const bool previewVisible = m_dockPreview->isVisible();
    const bool terminalVisible = m_dockTerminal->isVisible();
    const bool commandOutputVisible = m_dockCommandOutput->isVisible();

    applyDefaultDockLayout();

    m_dockSidebar->setVisible(true);
    m_dockLeftPane->setVisible(true);
    m_dockRightPane->setVisible(rightVisible);
    m_dockPreview->setVisible(previewVisible);
    m_dockTerminal->setVisible(terminalVisible);
    m_dockCommandOutput->setVisible(commandOutputVisible);

    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setSplitVisible(bool visible)
{
    if (visible && !m_dockRightPane->isVisible()) {
        m_rightPane->applySharedColumnLayout();
    }
    // Toggling the right pane must not change the folder-tree width: QMainWindow
    // redistributes the freed/needed width across the whole row. Pin the sidebar
    // to its current width during the relayout so the change is absorbed by the
    // file panes, then release the constraint so the user can still resize it.
    const bool pinSidebar = m_dockSidebar->isVisible() && !m_dockSidebar->isFloating();
    if (pinSidebar) {
        const int sidebarWidth = m_dockSidebar->width();
        m_dockSidebar->setFixedWidth(sidebarWidth);
    }
    m_dockRightPane->setVisible(visible);
    if (pinSidebar) {
        QTimer::singleShot(0, this, [this]() {
            m_dockSidebar->setMinimumWidth(0);
            m_dockSidebar->setMaximumWidth(QWIDGETSIZE_MAX);
        });
    }
    {
        const QSignalBlocker blocker(m_splitButton);
        m_splitButton->setChecked(visible);
    }
    if (m_splitAction) {
        const QSignalBlocker blocker(m_splitAction);
        m_splitAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setSidebarVisible(bool visible)
{
    m_dockSidebar->setVisible(visible);
    if (m_sidebarAction && m_sidebarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_sidebarAction);
        m_sidebarAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setPreviewVisible(bool visible)
{
    m_dockPreview->setVisible(visible);
    {
        const QSignalBlocker blocker(m_previewButton);
        m_previewButton->setChecked(visible);
    }
    if (m_previewAction) {
        const QSignalBlocker blocker(m_previewAction);
        m_previewAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setTerminalVisible(bool visible)
{
    m_dockTerminal->setVisible(visible);
    if (m_terminalButton && m_terminalButton->isChecked() != visible) {
        const QSignalBlocker blocker(m_terminalButton);
        m_terminalButton->setChecked(visible);
    }
    if (m_terminalAction && m_terminalAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_terminalAction);
        m_terminalAction->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}

void MainWindow::setCommandOutputVisible(bool visible)
{
    m_dockCommandOutput->setVisible(visible);
    if (m_commandOutputAction && m_commandOutputAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_commandOutputAction);
        m_commandOutputAction->setChecked(visible);
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

FilePane *MainWindow::activePane() const
{
    return m_activePane ? m_activePane : m_leftPane;
}
