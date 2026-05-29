#include "FilePane.h"
#include "UiText.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHeaderView>
#include <QInputDialog>
#include <QDirIterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QUrl>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QListWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
enum FileColumn {
    ColumnName = 0,
    ColumnType,
    ColumnSize,
    ColumnCreated,
    ColumnModified,
    ColumnMode,
    ColumnGit,
    kColumnCount
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
        return 92;
    default:
        return 120;
    }
}

QString permissionTriplet(QFile::Permissions permissions, QFile::Permission read, QFile::Permission write, QFile::Permission execute)
{
    QString text;
    text.append(permissions.testFlag(read) ? 'r' : '-');
    text.append(permissions.testFlag(write) ? 'w' : '-');
    text.append(permissions.testFlag(execute) ? 'x' : '-');
    return text;
}

QString modeString(const QFileInfo &info)
{
    QString text;
    if (info.isSymLink()) {
        text.append('l');
    } else if (info.isDir()) {
        text.append('d');
    } else {
        text.append('-');
    }

    const QFile::Permissions permissions = info.permissions();
    text.append(permissionTriplet(permissions, QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner));
    text.append(permissionTriplet(permissions, QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup));
    text.append(permissionTriplet(permissions, QFile::ReadOther, QFile::WriteOther, QFile::ExeOther));
    return text;
}

bool copyRecursively(const QString &sourcePath, const QString &destinationPath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir()) {
        QDir destinationDir(destinationPath);
        if (!destinationDir.exists() && !QDir().mkpath(destinationPath)) {
            return false;
        }

        QDirIterator iterator(sourcePath, QDir::NoDotAndDotDot | QDir::AllEntries);
        while (iterator.hasNext()) {
            iterator.next();
            const QString childSource = iterator.filePath();
            const QString childDestination = QDir(destinationPath).filePath(iterator.fileName());
            if (!copyRecursively(childSource, childDestination)) {
                return false;
            }
        }
        return true;
    }

    return QFile::copy(sourcePath, destinationPath);
}

QString uniquePathInDirectory(const QString &directory, const QString &baseName)
{
    const QString first = QDir(directory).filePath(baseName);
    if (!QFileInfo::exists(first)) {
        return first;
    }

    const QFileInfo info(first);
    for (int index = 2; ; ++index) {
        const QString name = info.suffix().isEmpty()
            ? QString("%1 %2").arg(info.completeBaseName()).arg(index)
            : QString("%1 %2.%3").arg(info.completeBaseName()).arg(index).arg(info.suffix());
        const QString candidate = QDir(directory).filePath(name);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
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
}

FileSystemProxyModel::FileSystemProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

int FileSystemProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return kColumnCount;
}

QVariant FileSystemProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ColumnName:
            return UiText::t("NAME", "NAME");
        case ColumnType:
            return UiText::t("TYPE", "TYPE");
        case ColumnSize:
            return UiText::t("SIZE", "SIZE");
        case ColumnCreated:
            return UiText::t("CREATED", "CREATED");
        case ColumnModified:
            return UiText::t("MODIFIED", "MODIFIED");
        case ColumnMode:
            return UiText::t("MODE", "MODE");
        case ColumnGit:
            return UiText::t("GIT", "GIT");
        default:
            break;
        }
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant FileSystemProxyModel::data(const QModelIndex &index, int role) const
{
    const auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel || !index.isValid()) {
        return QSortFilterProxyModel::data(index, role);
    }

    const QModelIndex sourceNameIndex = mapToSource(index.sibling(index.row(), ColumnName));
    const QFileInfo info = fsModel->fileInfo(sourceNameIndex);

    if (role == Qt::ForegroundRole) {
        return QColor(info.isDir() ? "#6FFF80" : "#CFFFCF");
    }

    if (role == Qt::DecorationRole && index.column() == ColumnName) {
        return fsModel->data(sourceNameIndex, role);
    }

    if (role == Qt::EditRole && index.column() == ColumnName) {
        return fsModel->data(sourceNameIndex, role);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColumnName:
            return fsModel->data(sourceNameIndex, role);
        case ColumnType:
            return fsModel->data(sourceNameIndex.sibling(sourceNameIndex.row(), 2), role);
        case ColumnSize:
            return fsModel->data(sourceNameIndex.sibling(sourceNameIndex.row(), 1), role);
        case ColumnCreated:
            return info.birthTime().isValid() ? info.birthTime().toString("yyyy-MM-dd HH:mm:ss") : QString();
        case ColumnModified:
            return info.lastModified().toString("yyyy-MM-dd HH:mm:ss");
        case ColumnMode:
            return modeString(info);
        case ColumnGit:
            return QString();
        default:
            break;
        }
    }
    return {};
}

Qt::ItemFlags FileSystemProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColumnName) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool FileSystemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    const QFileInfo leftInfo = fsModel->fileInfo(left.sibling(left.row(), 0));
    const QFileInfo rightInfo = fsModel->fileInfo(right.sibling(right.row(), 0));
    switch (sortColumn()) {
    case ColumnSize:
        return leftInfo.size() < rightInfo.size();
    case ColumnCreated:
        return leftInfo.birthTime() < rightInfo.birthTime();
    case ColumnModified:
        return leftInfo.lastModified() < rightInfo.lastModified();
    case ColumnMode:
        return modeString(leftInfo) < modeString(rightInfo);
    default:
        return QSortFilterProxyModel::lessThan(left, right);
    }
}

FilePane::FilePane(const QString &label, const QString &initialPath, QWidget *parent)
    : QWidget(parent),
      m_model(new QFileSystemModel(this)),
      m_proxyModel(new FileSystemProxyModel(this)),
      m_tabBar(new QTabBar(this)),
      m_view(new QTableView(this)),
      m_badgeLabel(new QLabel(this)),
      m_pathLabel(new QLabel(this)),
      m_statusLabel(new QLabel(this)),
      m_label(label)
{
    setObjectName("filePane");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(320);
    m_model->setRootPath("/");
    m_model->setFilter(QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files);
    m_model->setReadOnly(false);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);

    m_badgeLabel->setObjectName("paneBadge");
    m_pathLabel->setObjectName("panePath");
    m_statusLabel->setObjectName("paneStatus");
    m_badgeLabel->setText(m_label.toUpper());
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_tabBar->setObjectName("paneTabs");
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->addTab(tabTitleForPath(initialPath));
    m_tabBar->setTabData(0, QFileInfo(initialPath).absoluteFilePath());

    m_view->setModel(m_proxyModel);
    m_view->setObjectName("fileTable");
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setShowGrid(false);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_view->setIconSize(QSize(18, 18));
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_view->horizontalHeader()->setHighlightSections(false);
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
    restoreColumnSettings();

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(m_badgeLabel);
    headerLayout->addWidget(m_pathLabel, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addLayout(headerLayout);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_statusLabel);

    connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        m_view->setCurrentIndex(index);
        openSelected();
    });

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
                updatePreviewFromSelection();
                updateStatusLine();
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
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (m_tabBar->count() <= 1) {
            return;
        }
        m_tabBar->removeTab(index);
        if (m_tabBar->currentIndex() >= 0) {
            const QString path = m_tabBar->tabData(m_tabBar->currentIndex()).toString();
            if (!path.isEmpty()) {
                m_isSwitchingTabs = true;
                navigateTo(path, false);
                m_isSwitchingTabs = false;
            }
        }
    });

    navigateTo(initialPath, false);
    setActive(false);
}

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
    QStringList validPaths;
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir() && !validPaths.contains(path)) {
            validPaths.append(path);
        }
    }
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
    }
    const int clampedIndex = qBound(0, activeIndex, m_tabBar->count() - 1);
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
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
    for (const QModelIndex &index : rows) {
        const QString path = m_model->filePath(m_proxyModel->mapToSource(index));
        if (!seen.contains(path)) {
            urls.append(QUrl::fromLocalFile(path));
            seen.insert(path);
        }
    }
    return urls;
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
    m_model->setNameFilters(text.trimmed().isEmpty() ? QStringList() : QStringList(QString("*%1*").arg(text.trimmed())));
    m_model->setNameFilterDisables(false);
    updateStatusLine();
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
    m_pathLabel->setProperty("activePane", active);
    m_pathLabel->style()->unpolish(m_pathLabel);
    m_pathLabel->style()->polish(m_pathLabel);
}

void FilePane::navigateTo(const QString &path, bool recordHistory)
{
    QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isDir()) {
        emit statusMessageRequested(UiText::t("Cannot open directory: %1", "フォルダを開けません: %1").arg(path));
        return;
    }

    const QString nextPath = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    if (recordHistory && !m_currentPath.isEmpty() && m_currentPath != nextPath) {
        pushHistory(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = nextPath;
    m_pathLabel->setText(displayPath(m_currentPath));
    if (!m_isSwitchingTabs) {
        updateCurrentTabPath(m_currentPath);
    }
    m_view->setRootIndex(m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath)));
    updateStatusLine();
    emit directoryChanged(m_currentPath);
    emit activated(this);
}

void FilePane::goUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath());
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
    m_view->setRootIndex(m_proxyModel->mapFromSource(m_model->setRootPath(m_currentPath)));
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
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
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
        UiText::t("New Folder", "新規フォルダ"));
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
    const QString path = uniqueChildPath("New File.txt");
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
    for (const QUrl &url : urls) {
        QFile::moveToTrash(url.toLocalFile());
    }
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

void FilePane::pasteIntoCurrentDirectory()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime->hasUrls()) {
        return;
    }

    const bool move = mime->hasFormat("application/x-tfx-cut");
    for (const QUrl &url : mime->urls()) {
        const QString source = url.toLocalFile();
        if (source.isEmpty()) {
            continue;
        }
        const QString destination = QDir(m_currentPath).filePath(QFileInfo(source).fileName());
        if (QFileInfo::exists(destination)) {
            emit statusMessageRequested(UiText::t("Skipped existing item: %1", "既存項目をスキップしました: %1").arg(destination));
            continue;
        }
        if (move) {
            QFile::rename(source, destination);
        } else if (!copyRecursively(source, destination)) {
            emit statusMessageRequested(UiText::t("Could not paste item: %1", "項目をペーストできませんでした: %1").arg(source));
        }
    }
    updateStatusLine();
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
    const int index = m_tabBar->addTab(tabTitleForPath(m_currentPath));
    m_tabBar->setTabData(index, m_currentPath);
    m_tabBar->setCurrentIndex(index);
}

void FilePane::closeCurrentTab()
{
    const int index = m_tabBar->currentIndex();
    if (index < 0 || m_tabBar->count() <= 1) {
        return;
    }
    m_tabBar->removeTab(index);
    const int nextIndex = qMin(index, m_tabBar->count() - 1);
    if (nextIndex >= 0) {
        m_tabBar->setCurrentIndex(nextIndex);
    }
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
    if (watched == m_view) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            emit activated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
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
        openWith->addAction(UiText::t("Other...", "その他..."))->setEnabled(false);
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
    menu.addAction(UiText::t("Compress to Zip", "zip に圧縮"), this, &FilePane::compressSelectedItemsToZip)->setEnabled(hasSelection);
    if (isZip) {
        menu.addAction(UiText::t("Extract Zip", "zip を展開"), this, &FilePane::extractSelectedZip);
    }
    menu.addAction(UiText::t("Copy Items", "項目をコピー"), this, &FilePane::copySelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Cut Items", "項目をカット"), this, &FilePane::cutSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasUrls());

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

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showEmptyAreaContextMenu(const QPoint &point)
{
    QMenu menu(this);
    menu.addAction(UiText::t("New Folder", "新規フォルダ"), this, &FilePane::createFolder);
    menu.addAction(UiText::t("New File", "新規ファイル"), this, &FilePane::createFile);
    menu.addSeparator();
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasUrls());
    menu.addSeparator();
    menu.addAction(UiText::t("Select All", "すべて選択"), this, &FilePane::selectAllVisibleItems)
        ->setEnabled(m_proxyModel->rowCount(m_view->rootIndex()) > 0);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentPath));
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

    menu.exec(m_view->viewport()->mapToGlobal(point));
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
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
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
        : "Archive.zip";
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

void FilePane::restoreColumnSettings()
{
    QSettings settings;
    settings.beginGroup(QString("FilePane/%1/Columns").arg(m_label));

    auto *header = m_view->horizontalHeader();
    const QSignalBlocker headerBlocker(header);

    const QStringList visualOrder = settings.value("visualOrder").toStringList();
    if (visualOrder.size() == kColumnCount) {
        for (int visual = 0; visual < visualOrder.size(); ++visual) {
            bool ok = false;
            const int logical = visualOrder.at(visual).toInt(&ok);
            if (!ok || logical < 0 || logical >= kColumnCount) {
                continue;
            }
            const int currentVisual = header->visualIndex(logical);
            if (currentVisual >= 0 && currentVisual != visual) {
                header->moveSection(currentVisual, visual);
            }
        }
    } else {
        const QByteArray headerState = settings.value("headerState").toByteArray();
        if (!headerState.isEmpty()) {
            header->restoreState(headerState);
        }
    }

    const QList<QVariant> widths = settings.value("columnWidths").toList();
    for (int column = 0; column < kColumnCount; ++column) {
        const bool visible = settings.value(QString("column%1Visible").arg(column), true).toBool();
        m_view->setColumnHidden(column, column == 0 ? false : !visible);
        const int width = column < widths.size()
            ? widths.at(column).toInt()
            : settings.value(QString("column%1Width").arg(column), defaultColumnWidth(column)).toInt();
        header->resizeSection(column, std::max(48, width));
    }

    settings.endGroup();
}

void FilePane::saveColumnSettings()
{
    QSettings settings;
    settings.beginGroup(QString("FilePane/%1/Columns").arg(m_label));
    settings.setValue("headerState", m_view->horizontalHeader()->saveState());

    QStringList visualOrder;
    visualOrder.reserve(kColumnCount);
    for (int visual = 0; visual < kColumnCount; ++visual) {
        visualOrder.append(QString::number(m_view->horizontalHeader()->logicalIndex(visual)));
    }
    settings.setValue("visualOrder", visualOrder);

    QList<QVariant> widths;
    widths.reserve(kColumnCount);
    for (int column = 0; column < kColumnCount; ++column) {
        settings.setValue(QString("column%1Visible").arg(column), !m_view->isColumnHidden(column));
        const int width = m_view->horizontalHeader()->sectionSize(column);
        settings.setValue(QString("column%1Width").arg(column), width);
        widths.append(width);
    }
    settings.setValue("columnWidths", widths);
    settings.endGroup();
}

void FilePane::updatePreviewFromSelection()
{
    const QFileInfo info = currentFileInfo();
    if (info.exists()) {
        emit selectionPreviewRequested(info.absoluteFilePath());
    } else {
        emit selectionPreviewRequested(m_currentPath);
    }
}

void FilePane::updateStatusLine()
{
    const QModelIndex root = m_view->rootIndex();
    const int total = m_proxyModel->rowCount(root);
    const int selected = m_view->selectionModel()->selectedRows().size();
    QString selectedText = selected > 0
        ? UiText::t("%1 selected", "%1 件選択").arg(selected)
        : UiText::t("No selection", "選択なし");
    m_statusLabel->setText(UiText::t(" %1 of %2 items  |  %3  |  %4 ", " %1 / %2 件  |  %3  |  %4 ")
        .arg(qMin(total, qMax(0, m_view->currentIndex().row() + 1)))
        .arg(total)
        .arg(selectedText)
        .arg(displayPath(m_currentPath)));
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

QString FilePane::tabTitleForPath(const QString &path) const
{
    const QFileInfo info(path);
    const QString title = info.fileName();
    if (!title.isEmpty()) {
        return title;
    }
    return path == "/" ? "/" : path;
}

void FilePane::updateCurrentTabPath(const QString &path)
{
    int index = m_tabBar->currentIndex();
    if (index < 0) {
        index = m_tabBar->addTab(tabTitleForPath(path));
        m_tabBar->setCurrentIndex(index);
    }
    m_tabBar->setTabText(index, tabTitleForPath(path));
    m_tabBar->setTabData(index, path);
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
        m_view->setCurrentIndex(index);
    }
}
