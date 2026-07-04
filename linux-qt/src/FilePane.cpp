#include "FilePane.h"
#include "UiText.h"
#include "core/FileOperations.h"
#include "core/FileTypeInfo.h"
#include "core/GitService.h"
#include "core/TabState.h"
#include "controllers/GitStatusController.h"
#include "models/ColumnLayout.h"
#include "platform/Platform.h"
#include "views/FileViews.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QDirIterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QKeyEvent>
#include <QHash>
#include <QImage>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QSettings>
#include <QTemporaryDir>
#include <QSignalBlocker>
#include <QTimer>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QUrl>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QListView>
#include <QListWidget>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

using namespace tfx::core;
using namespace tfx::models;
using namespace tfx::platform;

namespace {
enum class ConflictChoice {
    Overwrite,
    Skip,
    Rename,
    Cancel,
};

int defaultColumnWidth(int column)
{
    switch (column) {
    case ColumnName:
        return 340;
    case ColumnType:
        return 120;
    case ColumnSize:
        return 96;
    case ColumnCreated:
        return 160;
    case ColumnModified:
        return 160;
    case ColumnMode:
        return 116;
    case ColumnGit:
        return 28;
    default:
        return 120;
    }
}

bool runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory, QString *errorText)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (errorText) {
            *errorText = process.errorString();
        }
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return true;
    }
    if (errorText) {
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        *errorText = stderrText.isEmpty() ? process.errorString() : stderrText;
    }
    return false;
}

bool removeExistingPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }
    return info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
}

QString firstUnsafeZipEntry(const QStringList &entries)
{
    for (const QString &entry : entries) {
        if (!tfx::platform::zipEntryPathIsSafe(entry)) {
            return entry;
        }
    }
    return QString();
}

ConflictChoice askConflict(QWidget *parent, const QString &fileName)
{
    QMessageBox box(parent);
    box.setWindowTitle(UiText::t("Name Conflict", "名前の衝突"));
    box.setText(UiText::t("An item named \"%1\" already exists.", "\"%1\" はすでに存在します。").arg(fileName));
    auto *overwrite = box.addButton(UiText::t("Overwrite", "上書き"), QMessageBox::DestructiveRole);
    auto *skip = box.addButton(UiText::t("Skip", "スキップ"), QMessageBox::RejectRole);
    auto *rename = box.addButton(UiText::t("Rename", "名前を変更"), QMessageBox::AcceptRole);
    auto *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(rename);
    box.exec();

    if (box.clickedButton() == overwrite) return ConflictChoice::Overwrite;
    if (box.clickedButton() == rename) return ConflictChoice::Rename;
    if (box.clickedButton() == cancel) return ConflictChoice::Cancel;
    if (box.clickedButton() == skip) return ConflictChoice::Skip;
    return ConflictChoice::Cancel;
}

bool clipboardCanPaste(const QMimeData *mime)
{
    return mime
        && (mime->hasUrls()
            || mime->hasImage()
            || mime->hasHtml()
            || mime->hasText()
            || mime->hasFormat("text/rtf"));
}

bool looksLikeDelimitedText(const QString &text, QChar separator)
{
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return false;
    }
    int expected = -1;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int count = line.count(separator);
        if (count <= 0) {
            return false;
        }
        if (expected < 0) {
            expected = count;
        } else if (count != expected) {
            return false;
        }
    }
    return expected > 0;
}

QString placeholderName(const QString &language, const QString &english, const QString &japanese)
{
    if (language == "en") {
        return english;
    }
    if (language == "ja") {
        return japanese;
    }
    return UiText::isJapanese() ? japanese : english;
}

QString plainTextClipboardBaseName(const QString &text, const QString &language)
{
    const QString trimmed = text.trimmed();
    const QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty()
        && (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp")) {
        return placeholderName(language, "Clipboard URL.url", "クリップボード URL.url");
    }
    if (looksLikeDelimitedText(text, '\t')) {
        return placeholderName(language, "Clipboard.tsv", "クリップボード.tsv");
    }
    if (looksLikeDelimitedText(text, ',')) {
        return placeholderName(language, "Clipboard.csv", "クリップボード.csv");
    }
    return placeholderName(language, "Clipboard.txt", "クリップボード.txt");
}

bool writeBytesToFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

QString shellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace('\'', "'\"'\"'");
    return "'" + quoted + "'";
}

QString scriptsDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("tfx/scripts");
}

QString expandCommandTokens(QString text,
                            const QStringList &paths,
                            const QString &cwd,
                            bool quoteValues)
{
    const QFileInfo first(paths.isEmpty() ? cwd : paths.first());
    const QString dir = paths.isEmpty() ? cwd : first.absolutePath();
    const QString ext = first.suffix();
    const auto value = [quoteValues](const QString &item) {
        return quoteValues ? shellQuote(item) : item;
    };

    QStringList quotedPaths;
    for (const QString &path : paths) {
        quotedPaths << value(path);
    }

    text.replace("{paths}", quotedPaths.join(' '));
    text.replace("{path}", value(paths.isEmpty() ? cwd : paths.first()));
    text.replace("{dir}", value(dir));
    text.replace("{name}", value(first.fileName()));
    text.replace("{stem}", value(first.completeBaseName()));
    text.replace("{ext}", value(ext));
    text.replace("{cwd}", value(cwd));
    text.replace("{scripts}", value(scriptsDirectory()));
    return text;
}

}

FilePane::FilePane(const QString &label, const QString &initialPath, QWidget *parent)
    : QWidget(parent),
      m_model(new QFileSystemModel(this)),
      m_proxyModel(new FileSystemProxyModel(this)),
      m_tabBar(new QTabBar(this)),
      m_view(new FileTableView(this)),
      m_badgeLabel(new QLabel(this)),
      m_pathEdit(new QLineEdit(this)),
      m_statusLabel(new QLabel(this)),
      m_label(label)
{
    setObjectName("filePane");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(220);
    m_model->setRootPath("/");
    m_model->setFilter(QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files);
    m_model->setReadOnly(false);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);

    m_badgeLabel->setObjectName("paneBadge");
    m_pathEdit->setObjectName("panePath");
    m_statusLabel->setObjectName("paneStatus");
    m_badgeLabel->setText(m_label.toUpper());
    m_pathEdit->setFrame(false);
    m_pathEdit->setClearButtonEnabled(false);
    m_pathEdit->setPlaceholderText("/");
    m_pathEdit->installEventFilter(this);
    m_tabBar->setObjectName("paneTabs");
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString initialTabPath = normalizedTabPath(initialPath);
    m_tabBar->addTab(tabTitleForPath(initialPath));
    m_tabBar->setTabData(0, initialTabPath);
    m_tabBar->setTabToolTip(0, initialTabPath);
    updateTabCloseButtons();

    m_view->setModel(m_proxyModel);
    m_view->setObjectName("fileTable");
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->viewport()->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropMode(QAbstractItemView::DragDrop);
    m_view->setDefaultDropAction(Qt::MoveAction);
    static_cast<FileTableView *>(m_view)->dropHandler =
        [this](const QList<QUrl> &urls, Qt::DropAction action, const QModelIndex &target) {
            QString targetDir = m_currentPath;
            if (target.isValid()) {
                const QModelIndex source = m_proxyModel->mapToSource(target.sibling(target.row(), 0));
                const QFileInfo info = m_model->fileInfo(source);
                if (info.isDir()) {
                    targetDir = info.absoluteFilePath();
                }
            }
            performDrop(urls, action, targetDir);
        };
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setShowGrid(false);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_view->setIconSize(QSize(18, 18));
    m_view->setItemDelegate(new FileItemDelegate(m_view));
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setMinimumSectionSize(24);
    m_view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->horizontalHeader()->setSortIndicatorShown(true);
    m_view->horizontalHeader()->setSectionsMovable(true);
    m_view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->horizontalHeader()->resizeSection(column, defaultColumnWidth(column));
    }
    m_view->verticalHeader()->hide();
    m_view->verticalHeader()->setDefaultSectionSize(26);
    m_view->setAlternatingRowColors(false);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->installEventFilter(this);
    applySharedColumnLayout();
    // QHeaderView resets the width of the synthesised extra columns
    // (modified/mode/git) to the default section size whenever the model is
    // reloaded or re-sorted, so the shared layout must be re-applied after
    // those events. The header's own reset slot runs before ours (it connects
    // first), so a synchronous re-apply wins.
    // While the model reorganises (sort/reload), QHeaderView resets the
    // synthesised extra columns to the default size and — in a shown window —
    // emits sectionResized for them. Those are not user actions, so saving is
    // suppressed for the whole layout-change window and the saved layout is
    // re-applied once it settles.
    const auto beginLayoutChange = [this]() { m_suppressColumnSave = true; };
    const auto endLayoutChange = [this]() {
        applySharedColumnLayout();
        QTimer::singleShot(0, this, [this]() { m_suppressColumnSave = false; });
    };
    connect(m_proxyModel, &QAbstractItemModel::layoutAboutToBeChanged, this, beginLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::modelAboutToBeReset, this, beginLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this, endLayoutChange);
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this, endLayoutChange);
    connect(m_model, &QFileSystemModel::directoryLoaded, this, [this](const QString &) {
        applySharedColumnLayout();
    });

    m_gitController = new GitStatusController(this);
    connect(m_gitController, &GitStatusController::statusesReady,
            this, [this](const QHash<QString, QString> &statuses) {
        m_proxyModel->setGitStatuses(statuses);
    });
    connect(m_gitController, &GitStatusController::branchChanged, this, [this](const QString &branch) {
        if (branch != m_gitBranch) {
            m_gitBranch = branch;
            updateStatusLine();
        }
    });

    // Auto-refresh: the file list itself is kept current by QFileSystemModel's
    // own watcher; this keeps the Git badges fresh when the directory changes,
    // plus a slow poll to catch git index changes (commits/staging).
    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(200);
    connect(m_refreshDebounce, &QTimer::timeout, this, [this]() { refreshGitStatuses(); });
    m_dirWatcher = new QFileSystemWatcher(this);
    connect(m_dirWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        m_refreshDebounce->start();
    });
    auto *gitPoll = new QTimer(this);
    gitPoll->setInterval(30000);
    connect(gitPoll, &QTimer::timeout, this, [this]() {
        if (isVisible() && !m_currentPath.isEmpty()) {
            refreshGitStatuses();
        }
    });
    gitPoll->start();

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(m_badgeLabel);
    headerLayout->addWidget(m_pathEdit, 1);

    m_searchModel = new QStandardItemModel(this);
    m_searchModel->setHorizontalHeaderLabels({
        columnTitle(ColumnName),
        columnTitle(ColumnType),
        columnTitle(ColumnSize),
        columnTitle(ColumnModified),
        columnTitle(ColumnMode),
    });
    m_searchView = new QTableView(this);
    m_searchView->setObjectName("fileTable");
    m_searchView->setModel(m_searchModel);
    m_searchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_searchView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_searchView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchView->setShowGrid(false);
    m_searchView->setFrameShape(QFrame::NoFrame);
    m_searchView->setIconSize(QSize(18, 18));
    m_searchView->setItemDelegate(new FileItemDelegate(m_searchView));
    m_searchView->verticalHeader()->hide();
    m_searchView->verticalHeader()->setDefaultSectionSize(26);
    m_searchView->horizontalHeader()->setStretchLastSection(true);
    m_searchView->setColumnWidth(0, 320);
    m_searchView->setColumnWidth(1, 120);
    m_searchView->setColumnWidth(2, 96);
    m_searchView->setColumnWidth(3, 160);
    m_searchView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_searchView->installEventFilter(this);

    m_iconView = new FileIconView(this);
    m_iconView->setObjectName("fileIcons");
    m_iconView->setModel(m_proxyModel);
    m_iconView->setSelectionModel(m_view->selectionModel());
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setWrapping(true);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setIconSize(QSize(48, 48));
    m_iconView->setGridSize(QSize(104, 80));
    m_iconView->setWordWrap(true);
    m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_iconView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_iconView->setFrameShape(QFrame::NoFrame);
    m_iconView->setDragEnabled(true);
    m_iconView->setAcceptDrops(true);
    m_iconView->viewport()->setAcceptDrops(true);
    m_iconView->setDropIndicatorShown(true);
    m_iconView->setDragDropMode(QAbstractItemView::DragDrop);
    m_iconView->setDefaultDropAction(Qt::MoveAction);
    m_iconView->installEventFilter(this);
    static_cast<FileIconView *>(m_iconView)->dropHandler =
        [this](const QList<QUrl> &urls, Qt::DropAction action, const QModelIndex &target) {
            QString targetDir = m_currentPath;
            if (target.isValid()) {
                const QModelIndex source = m_proxyModel->mapToSource(target.sibling(target.row(), 0));
                const QFileInfo info = m_model->fileInfo(source);
                if (info.isDir()) {
                    targetDir = info.absoluteFilePath();
                }
            }
            performDrop(urls, action, targetDir);
        };
    connect(m_iconView, &QListView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            m_iconView->setCurrentIndex(index);
            openSelected();
        }
    });

    m_zipModel = new QStandardItemModel(this);
    m_zipModel->setHorizontalHeaderLabels({columnTitle(ColumnName), columnTitle(ColumnType)});
    m_zipView = new QTableView(this);
    m_zipView->setObjectName("fileTable");
    m_zipView->setModel(m_zipModel);
    m_zipView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_zipView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_zipView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_zipView->setShowGrid(false);
    m_zipView->setFrameShape(QFrame::NoFrame);
    m_zipView->setIconSize(QSize(18, 18));
    m_zipView->setItemDelegate(new FileItemDelegate(m_zipView));
    m_zipView->verticalHeader()->hide();
    m_zipView->verticalHeader()->setDefaultSectionSize(26);
    m_zipView->horizontalHeader()->setStretchLastSection(true);
    m_zipView->setColumnWidth(0, 360);
    connect(m_zipView, &QTableView::activated, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString entry = m_zipModel->item(index.row(), 0)->data(Qt::UserRole).toString();
        const bool isDir = m_zipModel->item(index.row(), 0)->data(Qt::UserRole + 1).toBool();
        if (entry == "..") {
            if (m_zipDir.isEmpty()) {
                exitZipMode();
            } else {
                const int slash = m_zipDir.lastIndexOf('/', m_zipDir.size() - 2);
                m_zipDir = slash >= 0 ? m_zipDir.left(slash + 1) : QString();
                populateZipView();
            }
        } else if (isDir) {
            m_zipDir = entry;
            populateZipView();
        } else {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(false);
            if (tempDir.isValid()
                && tfx::platform::extractZipEntry(m_zipPath, entry, tempDir.path())) {
                tfx::platform::openPath(QDir(tempDir.path()).filePath(entry));
            } else {
                emit statusMessageRequested(UiText::t("Could not extract entry.", "エントリを展開できませんでした。"));
            }
        }
    });

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_view);
    m_viewStack->addWidget(m_iconView);
    m_viewStack->addWidget(m_searchView);
    m_viewStack->addWidget(m_zipView);
    m_viewStack->setCurrentWidget(m_view);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(0);
    connect(m_searchTimer, &QTimer::timeout, this, &FilePane::searchStep);
    const auto openSearchResult = [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString path = m_searchModel->item(index.row(), 0)->data(Qt::UserRole).toString();
        const QFileInfo info(path);
        if (!info.exists()) {
            return;
        }
        navigateTo(info.absolutePath());
        QTimer::singleShot(0, this, [this, path]() { setCurrentIndexForPath(path); });
    };
    connect(m_searchView, &QTableView::activated, this, openSearchResult);
    connect(m_searchView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
                updatePreviewFromSearchSelection();
                updateStatusLine();
            });
    connect(m_searchView, &QWidget::customContextMenuRequested,
            this, &FilePane::showSearchContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addLayout(headerLayout);
    layout->addWidget(m_viewStack, 1);
    layout->addWidget(m_statusLabel);

    connect(m_view, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        if (selectionDebugEnabled()) {
            selectionDebugLog(QString("[selection] clicked index=(%1,%2)")
                .arg(index.row())
                .arg(index.column()));
        }
        QTimer::singleShot(0, this, [this, index]() {
            if (!index.isValid()) {
                return;
            }
            emit activated(this);
            m_view->setFocus(Qt::MouseFocusReason);
            // Don't collapse a Ctrl/Shift multi-selection back to a single row.
            if (!(QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                m_view->setCurrentIndex(index);
                m_view->selectionModel()->select(rowSelection(index), QItemSelectionModel::ClearAndSelect);
                m_view->setProperty("currentSelectionRow", index.row());
                m_view->viewport()->update();
            }
            updatePreviewFromSelection();
            updateStatusLine();
            logSelectionState("after deferred clicked", m_view);
        });
    });

    connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        m_view->setCurrentIndex(index);
        openSelected();
    });

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &FilePane::commitPathEditor);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
                logSelectionState("selectionChanged", m_view);
                updatePreviewFromSelection();
                updateStatusLine();
            });
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex &previous) {
                if (current.isValid()) {
                    m_view->setProperty("currentSelectionRow", current.row());
                    m_view->viewport()->update();
                    updatePreviewFromSelection();
                    updateStatusLine();
                }
                if (!selectionDebugEnabled()) {
                    return;
                }
                selectionDebugLog(QString("[selection] currentChanged current=(%1,%2) previous=(%3,%4)")
                    .arg(current.row())
                    .arg(current.column())
                    .arg(previous.row())
                    .arg(previous.column()));
                logSelectionState("currentChanged", m_view);
            });

    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        if (m_view->indexAt(point).isValid()) {
            showFileContextMenu(point);
        } else {
            showEmptyAreaContextMenu(point);
        }
    });

    connect(m_view->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &FilePane::showColumnContextMenu);
    connect(m_view->horizontalHeader(), &QHeaderView::sectionMoved, this, [this]() {
        saveColumnSettings();
    });
    connect(m_view->horizontalHeader(), &QHeaderView::sectionResized, this, [this]() {
        saveColumnSettings();
    });
    connect(m_view->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this]() {
        saveColumnSettings();
    });
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        const QString path = m_tabBar->tabData(index).toString();
        if (!path.isEmpty() && path != m_currentPath) {
            m_isSwitchingTabs = true;
            navigateTo(path, false);
            m_isSwitchingTabs = false;
        }
        emit tabsChanged();
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (m_tabBar->count() <= 1) {
            return;
        }
        m_tabBar->removeTab(index);
        updateTabCloseButtons();
        if (m_tabBar->currentIndex() >= 0) {
            const QString path = m_tabBar->tabData(m_tabBar->currentIndex()).toString();
            if (!path.isEmpty()) {
                m_isSwitchingTabs = true;
                navigateTo(path, false);
                m_isSwitchingTabs = false;
            }
        }
        emit tabsChanged();
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this]() {
        emit tabsChanged();
    });
    connect(m_tabBar, &QWidget::customContextMenuRequested, this, &FilePane::showTabContextMenu);

    navigateTo(initialPath, false);
    setActive(false);
}

FilePane::~FilePane() = default;

QString FilePane::currentPath() const
{
    return m_currentPath;
}

QStringList FilePane::tabPaths() const
{
    QStringList paths;
    for (int index = 0; index < m_tabBar->count(); ++index) {
        paths.append(m_tabBar->tabData(index).toString());
    }
    return paths;
}

int FilePane::activeTabIndex() const
{
    return m_tabBar->currentIndex();
}

void FilePane::restoreTabs(const QStringList &paths, int activeIndex)
{
    const QStringList validPaths = normalizedTabPaths(paths);
    if (validPaths.isEmpty()) {
        return;
    }

    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(m_tabBar->count() - 1);
    }
    for (const QString &path : validPaths) {
        const int index = m_tabBar->addTab(tabTitleForPath(path));
        m_tabBar->setTabData(index, path);
        m_tabBar->setTabToolTip(index, path);
    }
    updateTabCloseButtons();
    const int clampedIndex = clampedTabIndex(activeIndex, m_tabBar->count());
    m_tabBar->setCurrentIndex(clampedIndex);
    m_tabBar->blockSignals(false);

    m_isSwitchingTabs = true;
    navigateTo(m_tabBar->tabData(clampedIndex).toString(), false);
    m_isSwitchingTabs = false;
}

QList<QUrl> FilePane::selectedUrls() const
{
    QList<QUrl> urls;
    QSet<QString> seen;
    QModelIndexList rows = m_view->selectionModel()->selectedRows();
    if (rows.isEmpty() && m_view->currentIndex().isValid()) {
        rows << m_view->currentIndex().sibling(m_view->currentIndex().row(), ColumnName);
    }
    for (const QModelIndex &index : rows) {
        const QString path = m_model->filePath(m_proxyModel->mapToSource(index));
        if (!seen.contains(path)) {
            urls.append(QUrl::fromLocalFile(path));
            seen.insert(path);
        }
    }
    return urls;
}

QStringList FilePane::selectedLocalPaths() const
{
    QStringList paths;
    for (const QUrl &url : selectedUrls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }
    return paths;
}

void FilePane::setUserCommands(const QList<UserCommand> &commands)
{
    m_userCommands = commands;
}

void FilePane::setOpenWithApplications(const QHash<QString, QString> &applications)
{
    m_openWithApplications = applications;
}

void FilePane::setPlaceholderLanguage(const QString &language)
{
    if (language == "en" || language == "ja" || language == "auto") {
        m_placeholderLanguage = language;
    }
}

void FilePane::runUserCommand(int index)
{
    if (index < 0 || index >= m_userCommands.size()) {
        return;
    }

    const UserCommand command = m_userCommands.at(index);
    if (command.name.trimmed().isEmpty() || command.command.trimmed().isEmpty()) {
        return;
    }

    const QStringList paths = selectedLocalPaths();
    if (command.requiresSelection && paths.isEmpty()) {
        emit statusMessageRequested(UiText::t("Select an item before running this command.",
                                              "このコマンドを実行するには項目を選択してください。"));
        return;
    }

    const QString workingDirectory = expandCommandTokens(command.workingDirectory, paths, m_currentPath, false);
    const QString expanded = expandCommandTokens(command.command, paths, m_currentPath, true);
    auto *process = new QProcess(this);
    const QString requestedWorkingDirectory = workingDirectory.isEmpty() ? m_currentPath : workingDirectory;
    const QString effectiveWorkingDirectory = tfx::core::canonicalDirectoryPath(requestedWorkingDirectory);
    if (effectiveWorkingDirectory.isEmpty()) {
        emit statusMessageRequested(UiText::t("Command working directory is not available: %1",
                                              "コマンドの作業ディレクトリを利用できません: %1")
            .arg(requestedWorkingDirectory));
        process->deleteLater();
        return;
    }
    process->setWorkingDirectory(effectiveWorkingDirectory);
    connect(process, &QProcess::finished, this,
            [this, process, command, expanded, effectiveWorkingDirectory](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString stdoutText = QString::fromLocal8Bit(process->readAllStandardOutput());
        const QString stderrText = QString::fromLocal8Bit(process->readAllStandardError());
        const bool failed = exitStatus != QProcess::NormalExit || exitCode != 0;
        emit commandOutputReady(command.name,
                                expanded,
                                effectiveWorkingDirectory,
                                exitCode,
                                exitStatus,
                                stdoutText,
                                stderrText,
                                command.showOutput || failed);
        if (!command.showOutput && !failed) {
            emit statusMessageRequested(UiText::t("Command finished: %1", "コマンド完了: %1").arg(command.name));
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, command](QProcess::ProcessError) {
        emit statusMessageRequested(UiText::t("Could not run command %1: %2",
                                              "コマンドを実行できませんでした %1: %2")
            .arg(command.name, process->errorString()));
    });
    process->start(tfx::platform::terminalShellProgram(), tfx::platform::terminalRunArguments(expanded));
}

void FilePane::setShowHiddenFiles(bool show)
{
    m_showHiddenFiles = show;
    QDir::Filters filters = QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files;
    if (show) {
        filters |= QDir::Hidden | QDir::System;
    }
    m_model->setFilter(filters);
    updateStatusLine();
}

void FilePane::setPathFilter(const QString &text)
{
    // Retained for compatibility; live filtering is no longer used. Searching is
    // started explicitly via startSearch() (Enter in the search box).
    Q_UNUSED(text);
}

void FilePane::setViewMode(bool iconMode)
{
    m_iconMode = iconMode;
    if (m_viewStack->currentWidget() != m_searchView) {
        m_viewStack->setCurrentWidget(iconMode ? static_cast<QWidget *>(m_iconView)
                                               : static_cast<QWidget *>(m_view));
    }
}

void FilePane::startSearch(const QString &term)
{
    cancelSearch();
    const QString trimmed = term.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_searchTerm = trimmed;
    m_searchMatches = 0;
    m_searchModel->removeRows(0, m_searchModel->rowCount());

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (m_showHiddenFiles) {
        filters |= QDir::Hidden | QDir::System;
    }
    m_searchIterator = new QDirIterator(m_currentPath, filters, QDirIterator::Subdirectories);
    m_viewStack->setCurrentWidget(m_searchView);
    updateStatusLine();
    emit statusMessageRequested(UiText::t("Searching: %1...", "検索中: %1...").arg(m_searchTerm));
    m_searchTimer->start();
}

void FilePane::searchStep()
{
    if (!m_searchIterator) {
        m_searchTimer->stop();
        return;
    }
    const QDir base(m_currentPath);
    int budget = 400;
    while (budget-- > 0 && m_searchIterator->hasNext()) {
        const QString path = m_searchIterator->next();
        if (!m_searchIterator->fileName().contains(m_searchTerm, Qt::CaseInsensitive)) {
            continue;
        }
        const QFileInfo info = m_searchIterator->fileInfo();

        auto *nameItem = new QStandardItem(m_iconProvider.icon(info), base.relativeFilePath(path));
        nameItem->setData(path, Qt::UserRole);
        nameItem->setForeground(QColor(info.isDir() ? m_directoryForeground : m_fileForeground));
        auto *typeItem = new QStandardItem(englishTypeName(info));
        auto *sizeItem = new QStandardItem(info.isDir() ? QString() : sizeString(info.size()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *modifiedItem = new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        auto *modeItem = new QStandardItem(modeString(info));
        m_searchModel->appendRow({nameItem, typeItem, sizeItem, modifiedItem, modeItem});
        ++m_searchMatches;
    }
    if (!m_searchIterator->hasNext()) {
        m_searchTimer->stop();
        delete m_searchIterator;
        m_searchIterator = nullptr;
        updateStatusLine();
        emit statusMessageRequested(UiText::t("Search complete: %1 matches", "検索完了: %1 件").arg(m_searchMatches));
    } else {
        updateStatusLine();
        emit statusMessageRequested(UiText::t("Searching: %1 matches", "検索中: %1 件").arg(m_searchMatches));
    }
}

void FilePane::cancelSearch()
{
    if (m_searchTimer) {
        m_searchTimer->stop();
    }
    if (m_searchIterator) {
        delete m_searchIterator;
        m_searchIterator = nullptr;
    }
    if (m_searchModel) {
        m_searchModel->removeRows(0, m_searchModel->rowCount());
    }
    if (m_viewStack && m_view) {
        m_viewStack->setCurrentWidget(m_iconMode ? static_cast<QWidget *>(m_iconView)
                                                 : static_cast<QWidget *>(m_view));
    }
}

void FilePane::openZip(const QString &path)
{
    m_zipPath = path;
    m_zipDir.clear();
    m_zipEntries = tfx::platform::listZipEntries(path);
    if (m_zipEntries.isEmpty()) {
        emit statusMessageRequested(UiText::t("Could not read archive.", "アーカイブを読み込めませんでした。"));
        return;
    }
    const QString unsafeEntry = firstUnsafeZipEntry(m_zipEntries);
    if (!unsafeEntry.isEmpty()) {
        m_zipEntries.clear();
        emit statusMessageRequested(
            UiText::t("Archive contains an unsafe path: %1", "アーカイブに安全でないパスが含まれています: %1")
                .arg(unsafeEntry));
        return;
    }
    populateZipView();
    m_viewStack->setCurrentWidget(m_zipView);
}

void FilePane::populateZipView()
{
    m_zipModel->removeRows(0, m_zipModel->rowCount());

    auto *up = new QStandardItem(m_iconProvider.icon(QFileIconProvider::Folder), "..");
    up->setData("..", Qt::UserRole);
    up->setData(true, Qt::UserRole + 1);
    m_zipModel->appendRow({up, new QStandardItem(QString())});

    QSet<QString> dirsSeen;
    const int prefixLen = m_zipDir.size();
    for (const QString &entry : m_zipEntries) {
        if (!entry.startsWith(m_zipDir)) {
            continue;
        }
        const QString rest = entry.mid(prefixLen);
        if (rest.isEmpty()) {
            continue;
        }
        const int slash = rest.indexOf('/');
        if (slash >= 0) {
            const QString dirName = rest.left(slash);
            const QString fullDir = m_zipDir + dirName + "/";
            if (!dirsSeen.contains(fullDir)) {
                dirsSeen.insert(fullDir);
                auto *item = new QStandardItem(m_iconProvider.icon(QFileIconProvider::Folder), dirName);
                item->setData(fullDir, Qt::UserRole);
                item->setData(true, Qt::UserRole + 1);
                m_zipModel->appendRow({item, new QStandardItem(UiText::t("Folder", "フォルダ"))});
            }
        } else {
            auto *item = new QStandardItem(m_iconProvider.icon(QFileIconProvider::File), rest);
            item->setData(entry, Qt::UserRole);
            item->setData(false, Qt::UserRole + 1);
            const QString suffix = QFileInfo(rest).suffix();
            m_zipModel->appendRow({item, new QStandardItem(suffix.isEmpty() ? UiText::t("File", "ファイル")
                                                                            : suffix.toUpper() + " file")});
        }
    }

    const QString internal = m_zipDir.isEmpty() ? QString() : "/" + m_zipDir.chopped(1);
    m_pathEdit->setText(displayPath(m_zipPath) + internal);
}

void FilePane::exitZipMode()
{
    const QString zip = m_zipPath;
    if (zip.isEmpty()) {
        return;
    }
    navigateTo(QFileInfo(zip).absolutePath());
    QTimer::singleShot(0, this, [this, zip]() { setCurrentIndexForPath(zip); });
}

void FilePane::setActive(bool active)
{
    m_isActive = active;
    setProperty("activePane", active);
    style()->unpolish(this);
    style()->polish(this);
    m_badgeLabel->setProperty("activePane", active);
    m_badgeLabel->style()->unpolish(m_badgeLabel);
    m_badgeLabel->style()->polish(m_badgeLabel);
    m_pathEdit->setProperty("activePane", active);
    m_pathEdit->style()->unpolish(m_pathEdit);
    m_pathEdit->style()->polish(m_pathEdit);
}

void FilePane::setThemeColors(const QString &fileForeground, const QString &directoryForeground)
{
    m_fileForeground = fileForeground;
    m_directoryForeground = directoryForeground;
    m_proxyModel->setThemeColors(fileForeground, directoryForeground);
}

void FilePane::navigateTo(const QString &path, bool recordHistory)
{
    QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isDir()) {
        emit statusMessageRequested(UiText::t("Cannot open directory: %1", "フォルダを開けません: %1").arg(path));
        return;
    }

    cancelSearch();
    m_zipPath.clear();
    m_zipDir.clear();
    m_zipEntries.clear();

    const QString nextPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    if (recordHistory && !m_currentPath.isEmpty() && m_currentPath != nextPath) {
        pushHistory(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = nextPath;
    m_pathEdit->setText(displayPath(m_currentPath));
    if (!m_isSwitchingTabs) {
        updateCurrentTabPath(m_currentPath);
    }
    const QModelIndex navRoot = m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath));
    m_view->setRootIndex(navRoot);
    m_iconView->setRootIndex(navRoot);
    m_view->setProperty("currentSelectionRow", -1);
    if (m_dirWatcher) {
        if (!m_dirWatcher->directories().isEmpty()) {
            m_dirWatcher->removePaths(m_dirWatcher->directories());
        }
        if (!m_currentPath.isEmpty()) {
            m_dirWatcher->addPath(m_currentPath);
        }
    }
    refreshGitStatuses();
    updateStatusLine();
    emit directoryChanged(m_currentPath);
    emit activated(this);
}

void FilePane::focusFileList()
{
    QWidget *target = m_view;
    if (m_viewStack && m_viewStack->currentWidget() == m_iconView) {
        target = m_iconView;
    }
    target->setFocus(Qt::OtherFocusReason);
}

void FilePane::goUp()
{
    const QString previousPath = m_currentPath;
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath());
        QTimer::singleShot(0, this, [this, previousPath]() {
            setCurrentIndexForPath(previousPath);
        });
    }
}

void FilePane::goBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }
    m_forwardStack.push(m_currentPath);
    navigateTo(m_backStack.pop(), false);
}

void FilePane::goForward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }
    m_backStack.push(m_currentPath);
    navigateTo(m_forwardStack.pop(), false);
}

void FilePane::reload()
{
    m_model->setRootPath(QString());
    const QModelIndex reloadRoot = m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath));
    m_view->setRootIndex(reloadRoot);
    m_iconView->setRootIndex(reloadRoot);
    m_view->setProperty("currentSelectionRow", -1);
    refreshGitStatuses();
    updateStatusLine();
}

void FilePane::openSelected()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    if (info.isDir()) {
        navigateTo(info.absoluteFilePath());
        QTimer::singleShot(0, this, [this]() {
            selectParentEntry();
        });
    } else if (info.suffix().compare("zip", Qt::CaseInsensitive) == 0) {
        openZip(info.absoluteFilePath());
    } else {
        openPath(info.absoluteFilePath());
    }
}

void FilePane::renameSelected()
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = m_view->selectionModel()->selectedRows();
        if (!rows.isEmpty()) {
            index = rows.first();
        }
    }
    if (!index.isValid()) {
        return;
    }
    m_view->edit(index.sibling(index.row(), ColumnName));
}

void FilePane::createFolder()
{
    const QString name = QInputDialog::getText(
        this,
        UiText::t("New Folder", "新規フォルダ"),
        UiText::t("Name:", "名前:"),
        QLineEdit::Normal,
        placeholderText("New Folder", "新規フォルダ"));
    if (name.isEmpty()) {
        return;
    }
    QDir dir(m_currentPath);
    if (!dir.mkdir(name)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create folder.", "フォルダを作成できませんでした。"));
    }
    updateStatusLine();
}

void FilePane::createFile()
{
    const QString path = uniqueChildPath(placeholderText("New File.txt", "新規ファイル.txt"));
    QFile file(path);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create file.", "ファイルを作成できませんでした。"));
        return;
    }
    file.close();
    setCurrentIndexForPath(path);
    updateStatusLine();
}

void FilePane::moveSelectedToTrash()
{
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) {
        return;
    }
    if (QMessageBox::question(
            this,
            UiText::t("Move to Trash", "ゴミ箱へ移動"),
            UiText::t("Move selected item(s) to trash?", "選択項目をゴミ箱へ移動しますか?")) != QMessageBox::Yes) {
        return;
    }
    QStringList paths;
    for (const QUrl &url : urls) {
        paths << url.toLocalFile();
    }
    moveToTrash(paths);
    updateStatusLine();
}

void FilePane::copySelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Copied selected item(s).", "選択項目をコピーしました。"));
}

void FilePane::cutSelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    mime->setData("application/x-tfx-cut", "1");
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Cut selected item(s).", "選択項目をカットしました。"));
}

void FilePane::performDrop(const QList<QUrl> &urls, Qt::DropAction action, const QString &targetDir)
{
    if (urls.isEmpty() || targetDir.isEmpty()) {
        return;
    }
    const bool move = (action == Qt::MoveAction);
    const QDir dir(targetDir);
    const QString targetCanonical = QFileInfo(targetDir).absoluteFilePath();
    QVector<FileOperationRequest> requests;

    for (const QUrl &url : urls) {
        const QString source = url.toLocalFile();
        if (source.isEmpty()) {
            continue;
        }
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.exists()) {
            continue;
        }
        // Moving into the same directory is a no-op; copying continues below
        // and is renamed automatically.
        if (move && sourceInfo.absolutePath() == targetCanonical) {
            continue;
        }
        // Never move/copy a folder into itself or one of its descendants.
        if (sourceInfo.isDir()
            && (targetCanonical + "/").startsWith(sourceInfo.absoluteFilePath() + "/")) {
            emit statusMessageRequested(UiText::t("Cannot transfer a folder into itself.", "フォルダを自身の中へは移動/コピーできません。"));
            continue;
        }
        QString destination = dir.filePath(sourceInfo.fileName());
        const bool samePath = QFileInfo(destination).absoluteFilePath() == sourceInfo.absoluteFilePath();
        if (samePath && move) {
            continue;
        }
        if (samePath && !move) {
            destination = uniquePathInDirectory(targetDir, sourceInfo.fileName());
        } else if (QFileInfo::exists(destination)) {
            const ConflictChoice choice = askConflict(this, sourceInfo.fileName());
            if (choice == ConflictChoice::Cancel) {
                break;
            }
            if (choice == ConflictChoice::Skip) {
                continue;
            }
            if (choice == ConflictChoice::Rename) {
                destination = uniquePathInDirectory(targetDir, sourceInfo.fileName());
            } else if (!removeExistingPath(destination)) {
                emit statusMessageRequested(UiText::t("Could not overwrite item: %1", "項目を上書きできませんでした: %1").arg(sourceInfo.fileName()));
                continue;
            }
        }

        requests.append({source, destination, move});
    }

    if (!requests.isEmpty()) {
        emit fileOperationRequested(requests);
    }
    updateStatusLine();
}

void FilePane::pasteIntoCurrentDirectory()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!clipboardCanPaste(mime)) {
        return;
    }

    bool sawLocalUrl = false;
    const bool move = mime->hasFormat("application/x-tfx-cut");
    if (mime->hasUrls()) {
        QVector<FileOperationRequest> requests;
        for (const QUrl &url : mime->urls()) {
            const QString source = url.toLocalFile();
            if (source.isEmpty()) {
                continue;
            }
            sawLocalUrl = true;
            const QFileInfo sourceInfo(source);
            if (!sourceInfo.exists()) {
                continue;
            }
            if (sourceInfo.isDir()
                && (QFileInfo(m_currentPath).absoluteFilePath() + "/").startsWith(sourceInfo.absoluteFilePath() + "/")) {
                emit statusMessageRequested(UiText::t("Cannot transfer a folder into itself.", "フォルダを自身の中へは移動/コピーできません。"));
                continue;
            }

            QString destination = QDir(m_currentPath).filePath(sourceInfo.fileName());
            const bool samePath = QFileInfo(destination).absoluteFilePath() == sourceInfo.absoluteFilePath();
            if (samePath && move) {
                continue;
            }
            if (samePath && !move) {
                destination = uniquePathInDirectory(m_currentPath, sourceInfo.fileName());
            } else if (QFileInfo::exists(destination)) {
                const ConflictChoice choice = askConflict(this, sourceInfo.fileName());
                if (choice == ConflictChoice::Cancel) {
                    break;
                }
                if (choice == ConflictChoice::Skip) {
                    continue;
                }
                if (choice == ConflictChoice::Rename) {
                    destination = uniquePathInDirectory(m_currentPath, sourceInfo.fileName());
                } else if (!removeExistingPath(destination)) {
                    emit statusMessageRequested(UiText::t("Could not overwrite item: %1", "項目を上書きできませんでした: %1").arg(sourceInfo.fileName()));
                    continue;
                }
            }

            requests.append({source, destination, move});
        }
        if (!requests.isEmpty()) {
            emit fileOperationRequested(requests);
        }
        updateStatusLine();
        if (sawLocalUrl) {
            return;
        }
    }

    pasteClipboardAsFile(false);
}

void FilePane::pasteClipboardAsPlainText()
{
    pasteClipboardAsFile(true);
}

bool FilePane::pasteClipboardAsFile(bool plainTextOnly)
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime) {
        return false;
    }

    QString baseName;
    QByteArray data;
    bool image = false;
    QImage clipboardImage;

    if (!plainTextOnly && mime->hasImage()) {
        clipboardImage = qvariant_cast<QImage>(mime->imageData());
        baseName = placeholderText("Clipboard Image.png", "クリップボード画像.png");
        image = !clipboardImage.isNull();
    } else if (!plainTextOnly && mime->hasFormat("text/rtf")) {
        baseName = placeholderText("Clipboard.rtf", "クリップボード.rtf");
        data = mime->data("text/rtf");
    } else if (!plainTextOnly && mime->hasHtml()) {
        baseName = placeholderText("Clipboard.html", "クリップボード.html");
        data = mime->html().toUtf8();
    } else if (mime->hasText()) {
        baseName = plainTextOnly
            ? placeholderText("Clipboard.txt", "クリップボード.txt")
            : plainTextClipboardBaseName(mime->text(), m_placeholderLanguage);
        data = mime->text().toUtf8();
    } else if (!plainTextOnly && mime->hasUrls() && !mime->urls().isEmpty()) {
        baseName = placeholderText("Clipboard URL.url", "クリップボード URL.url");
        data = mime->urls().first().toString().toUtf8();
    } else {
        return false;
    }

    if (baseName.isEmpty() || (!image && data.isEmpty())) {
        return false;
    }

    const QString path = uniquePathInDirectory(m_currentPath, baseName);
    const bool ok = image ? clipboardImage.save(path, "PNG") : writeBytesToFile(path, data);
    if (!ok) {
        emit statusMessageRequested(UiText::t("Could not create clipboard file.", "クリップボードからファイルを作成できませんでした。"));
        return false;
    }
    reload();
    setCurrentIndexForPath(path);
    emit statusMessageRequested(UiText::t("Created file from clipboard.", "クリップボードからファイルを作成しました。"));
    updateStatusLine();
    return true;
}

void FilePane::copySelectedPaths()
{
    QStringList paths;
    for (const QUrl &url : selectedUrls()) {
        paths.append(url.toLocalFile());
    }
    QApplication::clipboard()->setText(paths.join('\n'));
    emit statusMessageRequested(UiText::t("Copied absolute path(s).", "絶対パスをコピーしました。"));
}

void FilePane::newTab()
{
    const int existing = tabIndexForPath(m_currentPath);
    if (existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
        return;
    }

    const int index = m_tabBar->addTab(tabTitleForPath(m_currentPath));
    m_tabBar->setTabData(index, m_currentPath);
    m_tabBar->setTabToolTip(index, m_currentPath);
    m_tabBar->setCurrentIndex(index);
    updateTabCloseButtons();
    emit tabsChanged();
}

void FilePane::closeCurrentTab()
{
    const int index = m_tabBar->currentIndex();
    if (index < 0 || m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->removeTab(index);
    updateTabCloseButtons();
    const int nextIndex = qMin(index, m_tabBar->count() - 1);
    if (nextIndex >= 0) {
        m_tabBar->setCurrentIndex(nextIndex);
    }
    emit tabsChanged();
}

void FilePane::nextTab()
{
    if (m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % m_tabBar->count());
}

void FilePane::previousTab()
{
    if (m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->setCurrentIndex((m_tabBar->currentIndex() - 1 + m_tabBar->count()) % m_tabBar->count());
}

bool FilePane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pathEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_pathEdit->setText(m_currentPath);
            m_pathEdit->selectAll();
            emit activated(this);
        }
        if (event->type() == QEvent::FocusOut) {
            m_pathEdit->setText(displayPath(m_currentPath));
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_pathEdit->setText(displayPath(m_currentPath));
                m_view->setFocus();
                return true;
            }
        }
    }
    if (watched == m_view || watched == m_iconView || watched == m_searchView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            emit activated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (watched == m_searchView) {
                if (keyEvent->key() == Qt::Key_Escape) {
                    cancelSearch();
                    focusFileList();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    const QModelIndex current = m_searchView->currentIndex();
                    if (current.isValid()) {
                        emit m_searchView->activated(current);
                    }
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Backspace) {
                    cancelSearch();
                    goUp();
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Down
                && !m_view->currentIndex().isValid()
                && m_view->selectionModel()->selectedIndexes().isEmpty()) {
                if (selectParentEntry()) {
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                openSelected();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                goUp();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QModelIndex FilePane::currentSourceIndex() const
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = m_view->selectionModel()->selectedRows();
        if (!rows.isEmpty()) {
            index = rows.first();
        }
    }
    return index.isValid() ? m_proxyModel->mapToSource(index.sibling(index.row(), ColumnName)) : QModelIndex();
}

QFileInfo FilePane::currentFileInfo() const
{
    const QModelIndex index = currentSourceIndex();
    return index.isValid() ? m_model->fileInfo(index) : QFileInfo();
}

QString FilePane::searchResultPath(const QModelIndex &index) const
{
    if (!index.isValid() || !m_searchModel) {
        return QString();
    }
    QStandardItem *item = m_searchModel->item(index.row(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QStringList FilePane::selectedSearchResultPaths() const
{
    QStringList paths;
    QSet<QString> seen;
    if (!m_searchView) {
        return paths;
    }
    QModelIndexList rows = m_searchView->selectionModel()->selectedRows();
    if (rows.isEmpty() && m_searchView->currentIndex().isValid()) {
        rows << m_searchView->currentIndex();
    }
    for (const QModelIndex &index : rows) {
        const QString path = searchResultPath(index);
        if (!path.isEmpty() && !seen.contains(path)) {
            paths << path;
            seen.insert(path);
        }
    }
    return paths;
}

QString FilePane::uniqueChildPath(const QString &baseName) const
{
    QString path = QDir(m_currentPath).filePath(baseName);
    if (!QFileInfo::exists(path)) {
        return path;
    }
    const QFileInfo info(path);
    for (int i = 2; ; ++i) {
        const QString numberedName = info.suffix().isEmpty()
            ? QString("%1 %2").arg(info.completeBaseName()).arg(i)
            : QString("%1 %2.%3").arg(info.completeBaseName()).arg(i).arg(info.suffix());
        const QString candidate = QDir(m_currentPath).filePath(numberedName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
}

void FilePane::showFileContextMenu(const QPoint &point)
{
    const QModelIndex clicked = m_view->indexAt(point);
    if (clicked.isValid() && !m_view->selectionModel()->isSelected(clicked)) {
        m_view->selectRow(clicked.row());
    }

    const QFileInfo info = currentFileInfo();
    const bool hasSelection = info.exists();
    const bool isDirectory = hasSelection && info.isDir();
    const bool isZip = hasSelection && info.suffix().compare("zip", Qt::CaseInsensitive) == 0;

    QMenu menu(this);
    menu.addAction(UiText::t("Open", "開く"), this, &FilePane::openSelected)->setEnabled(hasSelection);

    if (hasSelection && !isDirectory) {
        auto *openWith = menu.addMenu(UiText::t("Open With", "このアプリケーションで開く"));
        openWith->addAction(UiText::t("Default Application", "既定のアプリケーション"), this, &FilePane::openSelected);
        const QString suffix = info.suffix().toLower();
        QStringList configuredPrograms;
        if (!suffix.isEmpty() && m_openWithApplications.contains(suffix)) {
            configuredPrograms << m_openWithApplications.value(suffix);
        }
        if (m_openWithApplications.contains("*")) {
            configuredPrograms << m_openWithApplications.value("*");
        }
        configuredPrograms.removeDuplicates();
        for (const QString &program : configuredPrograms) {
            if (program.trimmed().isEmpty()) {
                continue;
            }
            openWith->addAction(program, this, [this, program]() {
                openWithConfiguredApplication(program);
            });
        }
        if (!configuredPrograms.isEmpty()) {
            openWith->addSeparator();
        }
        openWith->addAction(UiText::t("Other...", "その他..."), this, &FilePane::openWithCustomApplication);
    }

    menu.addSeparator();
    menu.addAction(UiText::t("Move to Trash", "ゴミ箱へ移動"), this, &FilePane::moveSelectedToTrash)->setEnabled(hasSelection);

    auto *tagsMenu = menu.addMenu(UiText::t("Tags", "タグ"));
    tagsMenu->addAction(UiText::t("Add Custom Tag...", "カスタムタグを追加..."))->setEnabled(false);
    tagsMenu->addSeparator();
    tagsMenu->addAction("Red")->setEnabled(false);
    tagsMenu->addAction("Orange")->setEnabled(false);
    tagsMenu->addAction("Yellow")->setEnabled(false);
    tagsMenu->addAction("Green")->setEnabled(false);
    tagsMenu->addAction("Blue")->setEnabled(false);
    tagsMenu->addAction("Purple")->setEnabled(false);
    tagsMenu->addAction("Gray")->setEnabled(false);

    menu.addSeparator();
    menu.addAction(UiText::t("Rename", "名前を変更"), this, &FilePane::renameSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Create Link", "リンクを作成"), this, &FilePane::createLinkForSelection)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Compress to Zip", "zip に圧縮"), this, &FilePane::compressSelectedItemsToZip)->setEnabled(hasSelection);
    if (isZip) {
        menu.addAction(UiText::t("Extract Zip", "zip を展開"), this, &FilePane::extractSelectedZip);
    }
    menu.addAction(UiText::t("Copy Items", "項目をコピー"), this, &FilePane::copySelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Cut Items", "項目をカット"), this, &FilePane::cutSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(clipboardCanPaste(QApplication::clipboard()->mimeData()));
    menu.addAction(UiText::t("Paste as Plain Text", "プレーンテキストとしてペースト"), this, &FilePane::pasteClipboardAsPlainText)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasText());

    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, &FilePane::revealSelectionInFileManager)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, &FilePane::copySelectedPaths)->setEnabled(hasSelection);

    if (isDirectory) {
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, info]() {
            emit pinFolderRequested(info.absoluteFilePath());
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, &FilePane::openTerminalHere);
    }

    addUserCommandActions(&menu, hasSelection);

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showEmptyAreaContextMenu(const QPoint &point)
{
    QMenu menu(this);
    menu.addAction(UiText::t("New Folder", "新規フォルダ"), this, &FilePane::createFolder);
    menu.addAction(UiText::t("New File", "新規ファイル"), this, &FilePane::createFile);
    menu.addSeparator();
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(clipboardCanPaste(QApplication::clipboard()->mimeData()));
    menu.addAction(UiText::t("Paste as Plain Text", "プレーンテキストとしてペースト"), this, &FilePane::pasteClipboardAsPlainText)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasText());
    menu.addSeparator();
    menu.addAction(UiText::t("Select All", "すべて選択"), this, &FilePane::selectAllVisibleItems)
        ->setEnabled(m_proxyModel->rowCount(m_view->rootIndex()) > 0);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [this]() {
        revealInFileManager(m_currentPath);
    });
    menu.addAction(UiText::t("Copy Current Path", "現在のパスをコピー"), this, [this]() {
        QApplication::clipboard()->setText(m_currentPath);
    });
    menu.addSeparator();
    menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this]() {
        emit pinFolderRequested(m_currentPath);
    });
    menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this]() {
        emit openTerminalHereRequested(m_currentPath);
    });

    addUserCommandActions(&menu, false);

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showSearchContextMenu(const QPoint &point)
{
    const QModelIndex clicked = m_searchView->indexAt(point);
    if (clicked.isValid() && !m_searchView->selectionModel()->isSelected(clicked)) {
        m_searchView->selectRow(clicked.row());
    }

    const QStringList paths = selectedSearchResultPaths();
    const bool hasSelection = !paths.isEmpty();
    const QString firstPath = hasSelection ? paths.first() : QString();
    const QFileInfo firstInfo(firstPath);

    QMenu menu(this);
    menu.addAction(UiText::t("Open", "開く"), this, [this, clicked]() {
        QModelIndex index = clicked.isValid() ? clicked : m_searchView->currentIndex();
        if (index.isValid()) {
            emit m_searchView->activated(index);
        }
    })->setEnabled(hasSelection);
    menu.addAction(UiText::t("Go to Containing Folder", "含まれるフォルダへ移動"), this, [this, firstInfo]() {
        if (!firstInfo.exists()) {
            return;
        }
        const QString path = firstInfo.absoluteFilePath();
        navigateTo(firstInfo.absolutePath());
        QTimer::singleShot(0, this, [this, path]() { setCurrentIndexForPath(path); });
    })->setEnabled(hasSelection);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [firstPath]() {
        revealInFileManager(firstPath);
    })->setEnabled(hasSelection);
    menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, [paths]() {
        QApplication::clipboard()->setText(paths.join('\n'));
    })->setEnabled(hasSelection);

    menu.exec(m_searchView->viewport()->mapToGlobal(point));
}

void FilePane::showTabContextMenu(const QPoint &point)
{
    const int clicked = m_tabBar->tabAt(point);
    const bool hasTab = clicked >= 0;

    QMenu menu(this);
    menu.addAction(UiText::t("New Tab", "新規タブ"), this, &FilePane::newTab);
    auto *closeTab = menu.addAction(UiText::t("Close Tab", "タブを閉じる"), this, [this, clicked]() {
        if (clicked >= 0) {
            m_tabBar->setCurrentIndex(clicked);
        }
        closeCurrentTab();
    });
    closeTab->setEnabled(hasTab && m_tabBar->count() > 1);

    auto *closeOthers = menu.addAction(UiText::t("Close Other Tabs", "他のタブを閉じる"), this, [this, clicked]() {
        if (clicked < 0 || m_tabBar->count() <= 1) {
            return;
        }
        const QString path = m_tabBar->tabData(clicked).toString();
        const QString title = m_tabBar->tabText(clicked);
        const QString tooltip = m_tabBar->tabToolTip(clicked);

        m_tabBar->blockSignals(true);
        while (m_tabBar->count() > 0) {
            m_tabBar->removeTab(m_tabBar->count() - 1);
        }
        const int index = m_tabBar->addTab(title);
        m_tabBar->setTabData(index, path);
        m_tabBar->setTabToolTip(index, tooltip);
        m_tabBar->setCurrentIndex(index);
        m_tabBar->blockSignals(false);
        updateTabCloseButtons();
        if (!path.isEmpty() && path != m_currentPath) {
            m_isSwitchingTabs = true;
            navigateTo(path, false);
            m_isSwitchingTabs = false;
        }
        emit tabsChanged();
    });
    closeOthers->setEnabled(hasTab && m_tabBar->count() > 1);

    if (hasTab) {
        menu.addSeparator();
        menu.addAction(UiText::t("Copy Tab Path", "タブのパスをコピー"), this, [this, clicked]() {
            QApplication::clipboard()->setText(m_tabBar->tabData(clicked).toString());
        });
    }

    menu.exec(m_tabBar->mapToGlobal(point));
}

void FilePane::addUserCommandActions(QMenu *menu, bool hasSelection)
{
    if (m_userCommands.isEmpty()) {
        return;
    }

    QMenu *commandsMenu = nullptr;
    for (int i = 0; i < m_userCommands.size(); ++i) {
        const UserCommand &command = m_userCommands.at(i);
        if (command.name.trimmed().isEmpty() || command.command.trimmed().isEmpty()) {
            continue;
        }
        if (!commandsMenu) {
            menu->addSeparator();
            commandsMenu = menu->addMenu(UiText::t("Commands", "コマンド"));
        }
        auto *action = commandsMenu->addAction(command.name, this, [this, i]() {
            runUserCommand(i);
        });
        action->setEnabled(hasSelection || !command.requiresSelection);
        if (!command.shortcut.isEmpty()) {
            action->setShortcut(QKeySequence(command.shortcut));
        }
    }
}

void FilePane::showColumnContextMenu(const QPoint &point)
{
    QMenu menu(this);
    for (int column = 0; column < kColumnCount; ++column) {
        QAction *action = menu.addAction(columnTitle(column));
        action->setCheckable(true);
        action->setChecked(!m_view->isColumnHidden(column));
        action->setEnabled(column != 0);
        connect(action, &QAction::toggled, this, [this, column](bool visible) {
            m_view->setColumnHidden(column, !visible);
            saveColumnSettings();
        });
    }

    menu.addSeparator();
    menu.addAction(UiText::t("Column Settings...", "表示項目設定..."), this, &FilePane::showColumnSettingsDialog);
    menu.addAction(UiText::t("Reset Columns", "カラムをリセット"), this, &FilePane::resetColumns);

    menu.exec(m_view->horizontalHeader()->mapToGlobal(point));
}

QString FilePane::columnTitle(int column) const
{
    switch (column) {
    case ColumnName:
        return UiText::t("Name", "名前");
    case ColumnType:
        return UiText::t("Type", "種類");
    case ColumnSize:
        return UiText::t("Size", "サイズ");
    case ColumnCreated:
        return UiText::t("Date Created", "作成日時");
    case ColumnModified:
        return UiText::t("Date Modified", "更新日時");
    case ColumnMode:
        return UiText::t("File Mode", "ファイルモード");
    case ColumnGit:
        return UiText::t("Git Status", "Git ステータス");
    default:
        return {};
    }
}

void FilePane::resetColumns()
{
    const QSignalBlocker headerBlocker(m_view->horizontalHeader());
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        m_view->horizontalHeader()->moveSection(m_view->horizontalHeader()->visualIndex(column), column);
        m_view->horizontalHeader()->resizeSection(column, defaultColumnWidth(column));
    }
    saveColumnSettings();
}

void FilePane::showColumnSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(UiText::t("File List Settings", "ファイル一覧設定"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(UiText::t("Columns", "表示項目"), &dialog);
    title->setObjectName("sectionLabel");
    layout->addWidget(title);

    auto *list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    const auto addColumnItem = [this, list](int logical, bool visible) {
        auto *item = new QListWidgetItem(columnTitle(logical), list);
        item->setData(Qt::UserRole, logical);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        if (logical == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
    };
    const auto populateFromHeader = [this, list, addColumnItem]() {
        list->clear();
        for (int visual = 0; visual < kColumnCount; ++visual) {
            const int logical = m_view->horizontalHeader()->logicalIndex(visual);
            addColumnItem(logical, !m_view->isColumnHidden(logical));
        }
    };
    const auto populateDefaults = [list, addColumnItem]() {
        list->clear();
        for (int logical = 0; logical < kColumnCount; ++logical) {
            addColumnItem(logical, true);
        }
    };
    populateFromHeader();
    layout->addWidget(list);

    auto *moveLayout = new QHBoxLayout();
    auto *upButton = new QPushButton(UiText::t("Up", "上へ"), &dialog);
    auto *downButton = new QPushButton(UiText::t("Down", "下へ"), &dialog);
    auto *resetButton = new QPushButton(UiText::t("Reset", "リセット"), &dialog);
    moveLayout->addWidget(upButton);
    moveLayout->addWidget(downButton);
    moveLayout->addStretch(1);
    moveLayout->addWidget(resetButton);
    layout->addLayout(moveLayout);

    connect(upButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row > 0) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row - 1, item);
            list->setCurrentRow(row - 1);
        }
    });
    connect(downButton, &QPushButton::clicked, &dialog, [list]() {
        const int row = list->currentRow();
        if (row >= 0 && row < list->count() - 1) {
            QListWidgetItem *item = list->takeItem(row);
            list->insertItem(row + 1, item);
            list->setCurrentRow(row + 1);
        }
    });
    connect(resetButton, &QPushButton::clicked, &dialog, [populateDefaults]() {
        populateDefaults();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    for (int visual = 0; visual < list->count(); ++visual) {
        const int logical = list->item(visual)->data(Qt::UserRole).toInt();
        const int currentVisual = m_view->horizontalHeader()->visualIndex(logical);
        if (currentVisual != visual) {
            m_view->horizontalHeader()->moveSection(currentVisual, visual);
        }
        m_view->setColumnHidden(logical, logical == 0 ? false : list->item(visual)->checkState() != Qt::Checked);
    }
    saveColumnSettings();
}

void FilePane::revealSelectionInFileManager()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    revealInFileManager(info.absoluteFilePath());
}

void FilePane::createLinkForSelection()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    const QString linkPath = uniqueChildPath(info.fileName() + placeholderText(" link", " のリンク"));
    if (QFile::link(info.absoluteFilePath(), linkPath)) {
        QTimer::singleShot(0, this, [this, linkPath]() {
            setCurrentIndexForPath(linkPath);
        });
    } else {
        emit statusMessageRequested(UiText::t("Could not create link.", "リンクを作成できませんでした。"));
    }
}

void FilePane::openWithConfiguredApplication(const QString &program)
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists() || info.isDir() || program.trimmed().isEmpty()) {
        return;
    }
    if (!QProcess::startDetached(program, {info.absoluteFilePath()})) {
        emit statusMessageRequested(UiText::t("Could not launch: %1", "起動できませんでした: %1").arg(program));
    }
}

void FilePane::openWithCustomApplication()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }

    // Prefer the desktop's native "Open With" application chooser.
    if (openWithChooser(info.absoluteFilePath())) {
        return;
    }

    // Fallback: ask for a launch command when no native chooser is available.
    bool ok = false;
    const QString command = QInputDialog::getText(this,
        UiText::t("Open With", "このアプリケーションで開く"),
        UiText::t("Application command:", "アプリケーションのコマンド:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || command.isEmpty()) {
        return;
    }
    if (!QProcess::startDetached(command, {info.absoluteFilePath()})) {
        emit statusMessageRequested(UiText::t("Could not launch: %1", "起動できませんでした: %1").arg(command));
    }
}

void FilePane::openTerminalHere()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    emit openTerminalHereRequested(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
}

void FilePane::compressSelectedItemsToZip()
{
    const QString zipProgram = QStandardPaths::findExecutable("zip");
    if (zipProgram.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("zip command was not found.", "zip コマンドが見つかりません。"));
        return;
    }

    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) {
        return;
    }

    QString archiveBaseName = urls.size() == 1
        ? QFileInfo(urls.first().toLocalFile()).completeBaseName() + ".zip"
        : placeholderText("Archive.zip", "アーカイブ.zip");
    const QString archivePath = uniquePathInDirectory(m_currentPath, archiveBaseName);

    QStringList arguments;
    arguments << "-r" << archivePath;
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            arguments << QFileInfo(path).fileName();
        }
    }

    QString errorText;
    if (!runProcess(zipProgram, arguments, m_currentPath, &errorText)) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Could not create zip archive.\n%1", "zip アーカイブを作成できませんでした。\n%1").arg(errorText));
        return;
    }

    reload();
    setCurrentIndexForPath(archivePath);
    emit statusMessageRequested(UiText::t("Created zip archive.", "zip アーカイブを作成しました。"));
}

void FilePane::extractSelectedZip()
{
    const QString unzipProgram = QStandardPaths::findExecutable("unzip");
    if (unzipProgram.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("unzip command was not found.", "unzip コマンドが見つかりません。"));
        return;
    }

    const QFileInfo archiveInfo = currentFileInfo();
    if (!archiveInfo.exists() || archiveInfo.suffix().compare("zip", Qt::CaseInsensitive) != 0) {
        return;
    }

    const QString destinationPath = uniquePathInDirectory(m_currentPath, archiveInfo.completeBaseName());
    const QStringList entries = tfx::platform::listZipEntries(archiveInfo.absoluteFilePath());
    if (entries.isEmpty()) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not read archive.", "アーカイブを読み込めませんでした。"));
        return;
    }
    const QString unsafeEntry = firstUnsafeZipEntry(entries);
    if (!unsafeEntry.isEmpty()) {
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Archive contains an unsafe path and was not extracted:\n%1",
                      "アーカイブに安全でないパスが含まれているため展開しませんでした:\n%1")
                .arg(unsafeEntry));
        return;
    }

    if (!QDir().mkpath(destinationPath)) {
        QMessageBox::warning(this, "tfx", UiText::t("Could not create extraction folder.", "展開先フォルダを作成できませんでした。"));
        return;
    }

    QString errorText;
    const QStringList arguments = { archiveInfo.absoluteFilePath(), "-d", destinationPath };
    if (!runProcess(unzipProgram, arguments, m_currentPath, &errorText)) {
        QDir(destinationPath).removeRecursively();
        QMessageBox::warning(
            this,
            "tfx",
            UiText::t("Could not extract zip archive.\n%1", "zip アーカイブを展開できませんでした。\n%1").arg(errorText));
        return;
    }

    reload();
    setCurrentIndexForPath(destinationPath);
    emit statusMessageRequested(UiText::t("Extracted zip archive.", "zip アーカイブを展開しました。"));
}

void FilePane::selectAllVisibleItems()
{
    m_view->selectAll();
    updateStatusLine();
}

void FilePane::applyDefaultColumns()
{
    auto *header = m_view->horizontalHeader();
    for (int column = 0; column < kColumnCount; ++column) {
        m_view->setColumnHidden(column, false);
        header->moveSection(header->visualIndex(column), column);
        header->resizeSection(column, defaultColumnWidth(column));
    }
}

void FilePane::applySharedColumnLayout()
{
    // Column layout (order/visibility/width) is shared between panes: only the
    // left pane persists it and the right pane mirrors the left, so a single
    // (non per-pane) settings group is used. "V2" retires older layouts.
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");
    const bool hasSaved = settings.contains("width0");

    auto *header = m_view->horizontalHeader();
    const QSignalBlocker headerBlocker(header);

    if (!hasSaved) {
        settings.endGroup();
        applyDefaultColumns();
        return;
    }

    const QStringList order = normalizedColumnOrder(settings.value("order").toStringList());
    for (int visual = 0; visual < kColumnCount; ++visual) {
        const int logical = order.at(visual).toInt();
        const int current = header->visualIndex(logical);
        if (current >= 0 && current != visual) {
            header->moveSection(current, visual);
        }
    }

    for (int column = 0; column < kColumnCount; ++column) {
        const bool visible = settings.value(QString("visible%1").arg(column), true).toBool();
        m_view->setColumnHidden(column, column == ColumnName ? false : !visible);
        const int width = settings.value(QString("width%1").arg(column), defaultColumnWidth(column)).toInt();
        header->resizeSection(column, normalizedColumnWidth(width, defaultColumnWidth(column)));
    }
    const int sortColumn = normalizedSortColumn(settings.value("sortColumn", ColumnName).toInt());
    const Qt::SortOrder sortOrder = normalizedSortOrder(
        settings.value("sortOrder", static_cast<int>(Qt::AscendingOrder)).toInt());
    settings.endGroup();

    m_view->sortByColumn(sortColumn, sortOrder);
}

void FilePane::saveColumnSettings()
{
    // Ignore section changes triggered by the model reorganising itself rather
    // than by the user (those would persist bogus reset widths).
    if (m_suppressColumnSave) {
        return;
    }
    // Only the left pane owns the shared column layout.
    if (m_label != "LEFT") {
        return;
    }
    auto *header = m_view->horizontalHeader();
    QSettings settings;
    settings.beginGroup("FilePane/ColumnsV2");

    QStringList order;
    order.reserve(kColumnCount);
    for (int visual = 0; visual < kColumnCount; ++visual) {
        order.append(QString::number(header->logicalIndex(visual)));
    }
    settings.setValue("order", order);

    for (int column = 0; column < kColumnCount; ++column) {
        settings.setValue(QString("visible%1").arg(column), !m_view->isColumnHidden(column));
        settings.setValue(QString("width%1").arg(column), header->sectionSize(column));
    }
    settings.setValue("sortColumn", header->sortIndicatorSection());
    settings.setValue("sortOrder", static_cast<int>(header->sortIndicatorOrder()));
    settings.endGroup();
}

void FilePane::refreshGitStatuses()
{
    m_gitController->refresh(m_currentPath);
}

void FilePane::updatePreviewFromSelection()
{
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
    if (rows.size() > 1) {
        QStringList paths;
        for (const QModelIndex &index : rows) {
            const QModelIndex source = m_proxyModel->mapToSource(index.sibling(index.row(), 0));
            const QString path = m_model->filePath(source);
            if (!path.isEmpty()) {
                paths << path;
            }
        }
        if (paths.size() > 1) {
            emit multiSelectionPreviewRequested(paths);
            return;
        }
    }

    const QFileInfo info = currentFileInfo();
    if (info.exists()) {
        emit selectionPreviewRequested(info.absoluteFilePath());
    } else {
        emit selectionPreviewRequested(m_currentPath);
    }
}

void FilePane::updatePreviewFromSearchSelection()
{
    if (!m_searchView || !m_searchModel) {
        return;
    }

    const QModelIndexList rows = m_searchView->selectionModel()->selectedRows();
    if (rows.size() > 1) {
        QStringList paths;
        for (const QModelIndex &index : rows) {
            const QString path = m_searchModel->item(index.row(), 0)->data(Qt::UserRole).toString();
            if (QFileInfo::exists(path)) {
                paths << path;
            }
        }
        if (paths.size() > 1) {
            emit multiSelectionPreviewRequested(paths);
            return;
        }
    }

    QModelIndex index = m_searchView->currentIndex();
    if (!index.isValid() && !rows.isEmpty()) {
        index = rows.first();
    }
    if (!index.isValid()) {
        emit selectionPreviewRequested(m_currentPath);
        return;
    }

    const QString path = m_searchModel->item(index.row(), 0)->data(Qt::UserRole).toString();
    emit selectionPreviewRequested(QFileInfo::exists(path) ? path : m_currentPath);
}

void FilePane::updateStatusLine()
{
    if (m_viewStack && m_viewStack->currentWidget() == m_searchView) {
        const int total = m_searchModel ? m_searchModel->rowCount() : 0;
        int selected = m_searchView->selectionModel()->selectedRows().size();
        if (selected == 0 && m_searchView->currentIndex().isValid()) {
            selected = 1;
        }
        const QString selectedText = selected > 0
            ? UiText::t("%1 selected", "%1 件選択").arg(selected)
            : UiText::t("No selection", "選択なし");
        const QString searchText = m_searchIterator
            ? UiText::t("Searching \"%1\"", "\"%1\" を検索中").arg(m_searchTerm)
            : UiText::t("Search \"%1\"", "\"%1\" の検索結果").arg(m_searchTerm);
        m_statusLabel->setText(
            UiText::t(" %1 matches  |  %2  |  %3 ", " %1 件一致  |  %2  |  %3 ")
                .arg(total)
                .arg(selectedText)
                .arg(searchText));
        return;
    }

    const QModelIndex root = m_view->rootIndex();
    const int total = m_proxyModel->rowCount(root);
    int selected = m_view->selectionModel()->selectedRows().size();
    if (selected == 0 && m_view->currentIndex().isValid()) {
        selected = 1;
    }
    QString selectedText = selected > 0
        ? UiText::t("%1 selected", "%1 件選択").arg(selected)
        : UiText::t("No selection", "選択なし");
    QString text = UiText::t(" %1 of %2 items  |  %3 ", " %1 / %2 件  |  %3 ")
        .arg(qMin(total, qMax(0, m_view->currentIndex().row() + 1)))
        .arg(total)
        .arg(selectedText);
    if (!m_gitBranch.isEmpty()) {
        text += QString("  |  ⎇ %1 ").arg(m_gitBranch);
    }
    m_statusLabel->setText(text);
}

void FilePane::commitPathEditor()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_pathEdit->setText(displayPath(m_currentPath));
        return;
    }
    const QString expandedPath = path == "~"
        ? QDir::homePath()
        : (path.startsWith("~/") ? QDir::home().filePath(path.mid(2)) : path);
    navigateTo(expandedPath);
    m_pathEdit->setText(displayPath(m_currentPath));
}

QString FilePane::displayPath(const QString &path) const
{
    const QString home = QDir::homePath();
    if (path == home) {
        return "~";
    }
    if (path.startsWith(home + "/")) {
        return "~" + path.mid(home.size());
    }
    return path;
}

QString FilePane::placeholderText(const QString &english, const QString &japanese) const
{
    return placeholderName(m_placeholderLanguage, english, japanese);
}

QString FilePane::tabTitleForPath(const QString &path) const
{
    const QFileInfo info(path);
    const QString title = info.fileName();
    if (!title.isEmpty()) {
        return title;
    }
    return path == "/" ? "/" : path;
}

int FilePane::tabIndexForPath(const QString &path) const
{
    const QString normalized = normalizedTabPath(path);
    if (normalized.isEmpty()) {
        return -1;
    }
    for (int index = 0; index < m_tabBar->count(); ++index) {
        if (m_tabBar->tabData(index).toString() == normalized) {
            return index;
        }
    }
    return -1;
}

void FilePane::updateCurrentTabPath(const QString &path)
{
    const QString normalized = normalizedTabPath(path);
    if (normalized.isEmpty()) {
        return;
    }

    int index = m_tabBar->currentIndex();
    if (index < 0) {
        index = m_tabBar->addTab(tabTitleForPath(path));
        m_tabBar->setCurrentIndex(index);
    }
    const int existing = tabIndexForPath(normalized);
    if (existing >= 0 && existing != index) {
        const int removed = index;
        m_tabBar->blockSignals(true);
        m_tabBar->removeTab(removed);
        m_tabBar->blockSignals(false);
        const int adjustedExisting = existing > removed ? existing - 1 : existing;
        m_tabBar->setCurrentIndex(adjustedExisting);
        updateTabCloseButtons();
        emit tabsChanged();
        return;
    }

    m_tabBar->setTabText(index, tabTitleForPath(normalized));
    m_tabBar->setTabData(index, normalized);
    m_tabBar->setTabToolTip(index, normalized);
    updateTabCloseButtons();
    emit tabsChanged();
}

void FilePane::updateTabCloseButtons()
{
    m_tabBar->setTabsClosable(m_tabBar->count() > 1);
}

void FilePane::pushHistory(const QString &path)
{
    if (m_backStack.isEmpty() || m_backStack.top() != path) {
        m_backStack.push(path);
    }
}

void FilePane::setCurrentIndexForPath(const QString &path)
{
    const QModelIndex index = m_proxyModel->mapFromSource(m_model->index(path));
    if (index.isValid()) {
        selectProxyIndex(index);
    }
}

void FilePane::selectProxyIndex(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    const QModelIndex nameIndex = index.sibling(index.row(), ColumnName);
    m_view->setCurrentIndex(nameIndex);
    m_view->selectionModel()->select(rowSelection(nameIndex), QItemSelectionModel::ClearAndSelect);
    m_view->setProperty("currentSelectionRow", nameIndex.row());
    m_view->scrollTo(nameIndex, QAbstractItemView::PositionAtCenter);
    m_view->viewport()->update();
    updatePreviewFromSelection();
    updateStatusLine();
}

bool FilePane::selectParentEntry()
{
    const QModelIndex root = m_view->rootIndex();
    const int rows = m_proxyModel->rowCount(root);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_proxyModel->index(row, ColumnName, root);
        if (m_proxyModel->data(index, Qt::DisplayRole).toString() == "..") {
            selectProxyIndex(index);
            return true;
        }
    }

    if (rows > 0) {
        selectProxyIndex(m_proxyModel->index(0, ColumnName, root));
        return true;
    }
    return false;
}
