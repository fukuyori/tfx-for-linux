#include "MainWindow.h"
#include "UiText.h"
#include "core/SearchState.h"

#include <QAction>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

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
}

QString MainWindow::rightPaneStartupDirectory() const
{
    const QString configured = m_config.startup.resolvedRightFolder();
    return configured.isEmpty() ? m_leftPane->currentPath() : configured;
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

    const QByteArray state = settings.value("MainWindow/state").toByteArray();
    if (!state.isEmpty()) {
        restoreState(state, 1);
    }
    const QByteArray splitterState = settings.value("MainWindow/paneSplitter").toByteArray();
    if (!splitterState.isEmpty()) {
        m_paneSplitter->restoreState(splitterState);
    }

    m_dockSidebar->setVisible(sidebarVisible);
    m_dockFilePanes->setVisible(true);
    m_rightPane->setVisible(splitVisible);
    m_dockPreview->setVisible(previewVisible);
    m_dockTerminal->setVisible(terminalVisible);
    m_dockCommandOutput->setVisible(commandOutputVisible);

    // Only the left pane resumes its session. The right pane never reopens
    // where it left off, so it has no tabs to restore either.
    m_leftPane->restoreTabs(
        settings.value("Tabs/leftPaths").toStringList(),
        settings.value("Tabs/leftActiveIndex", 0).toInt());
    if (QFileInfo(m_initialPath).isDir()) {
        m_leftPane->navigateTo(m_initialPath, false);
    }
    m_rightPane->clearHistory();
    m_rightPane->navigateTo(rightPaneStartupDirectory(), false);

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

    m_pinnedCollapsed = settings.value("View/sidebarPinnedCollapsed", false).toBool();
    m_disksCollapsed = settings.value("View/sidebarDisksCollapsed", false).toBool();
    m_foldersCollapsed = settings.value("View/sidebarFoldersCollapsed", false).toBool();
    applySidebarSectionStates();

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
    settings.setValue("MainWindow/state", saveState(1));
    settings.setValue("MainWindow/paneSplitter", m_paneSplitter->saveState());
    settings.setValue("View/sidebarVisible", m_dockSidebar->isVisible());
    settings.setValue("View/splitVisible", !m_rightPane->isHidden());
    settings.setValue("View/previewVisible", m_dockPreview->isVisible());
    settings.setValue("View/terminalVisible", m_dockTerminal->isVisible());
    settings.setValue("View/commandOutputVisible", m_dockCommandOutput->isVisible());
    settings.setValue("View/showHiddenFiles", m_showHiddenFiles);
    settings.setValue("View/sidebarPinnedCollapsed", m_pinnedCollapsed);
    settings.setValue("View/sidebarDisksCollapsed", m_disksCollapsed);
    settings.setValue("View/sidebarFoldersCollapsed", m_foldersCollapsed);
    settings.setValue("View/leftIconMode", m_leftPane->isIconMode());
    settings.setValue("View/rightIconMode", m_rightPane->isIconMode());
    settings.setValue("Panes/activePane", activePane() == m_rightPane ? "RIGHT" : "LEFT");
    settings.setValue("Tabs/leftPaths", m_leftPane->tabPaths());
    settings.setValue("Tabs/leftActiveIndex", m_leftPane->activeTabIndex());
    // The right pane's tabs are deliberately not persisted: it always opens on
    // rightPaneStartupDirectory(), so saved tabs would only be stale clutter.
    settings.remove("Tabs/rightPaths");
    settings.remove("Tabs/rightActiveIndex");
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

// Watches config.toml for saves and re-applies it live. The watch is
// directory-level as well, so editors that save atomically (write to a temp
// file, then rename over the original) are still caught even though the
// original file node disappears.
void MainWindow::setupConfigWatcher()
{
    m_configReloadDebounce = new QTimer(this);
    m_configReloadDebounce->setSingleShot(true);
    m_configReloadDebounce->setInterval(400);
    connect(m_configReloadDebounce, &QTimer::timeout, this, &MainWindow::reloadConfig);

    m_configWatcher = new QFileSystemWatcher(this);
    const QString configFile = AppConfig::configFilePath();
    m_configWatcher->addPath(QFileInfo(configFile).absolutePath());
    if (QFileInfo::exists(configFile)) {
        m_configWatcher->addPath(configFile);
    }
    const auto scheduleReload = [this](const QString &) {
        m_configReloadDebounce->start();
    };
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged, this, scheduleReload);
    connect(m_configWatcher, &QFileSystemWatcher::directoryChanged, this, scheduleReload);
}

// Reloads config.toml and re-applies everything that can change at runtime:
// shortcuts (menus are rebuilt), colors, opacity, fonts, user commands,
// open-with entries, and preview settings. [startup] keeps its launch-time
// semantics.
void MainWindow::reloadConfig()
{
    // Editors that save by delete-then-write leave a window with no file;
    // reloading then would let loadOrCreate() recreate the default config
    // over the user's settings. Wait for the rewrite to land instead.
    if (!QFileInfo::exists(AppConfig::configFilePath())) {
        m_configReloadDebounce->start();
        return;
    }

    m_config = AppConfig::loadOrCreate();

    m_leftPane->setUserCommands(m_config.commands);
    m_rightPane->setUserCommands(m_config.commands);
    m_leftPane->setOpenWithApplications(m_config.openWith);
    m_rightPane->setOpenWithApplications(m_config.openWith);
    m_leftPane->setPlaceholderLanguage(m_config.naming.placeholderLanguage);
    m_rightPane->setPlaceholderLanguage(m_config.naming.placeholderLanguage);
    applyTerminalTheme();

    // Rebuild the menu bar (and the Commands menu) so shortcut and command
    // changes take effect, then restore the rebuilt toggles' checked state.
    const QList<QMenu *> menus = menuBar()->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);
    menuBar()->clear();
    qDeleteAll(menus);
    buildActions();
    setupConfigShortcuts();
    const auto sync = [](QAction *action, bool on) {
        if (action) {
            const QSignalBlocker blocker(action);
            action->setChecked(on);
        }
    };
    sync(m_splitAction, !m_rightPane->isHidden());
    sync(m_sidebarAction, m_dockSidebar->isVisible());
    sync(m_previewAction, m_dockPreview->isVisible());
    sync(m_terminalAction, m_dockTerminal->isVisible());
    sync(m_commandOutputAction, m_dockCommandOutput->isVisible());
    sync(m_hiddenAction, m_showHiddenFiles);
    syncIconViewToggle();

    // An atomic save replaced the file node, which drops it from the watcher.
    const QString configFile = AppConfig::configFilePath();
    if (m_configWatcher && !m_configWatcher->files().contains(configFile)
        && QFileInfo::exists(configFile)) {
        m_configWatcher->addPath(configFile);
    }

    showConfigWarnings();
    const bool wantsIntegrated = m_config.window.titleBar == QLatin1String("integrated");
    if (wantsIntegrated != m_integratedTitleBar) {
        statusBar()->showMessage(
            UiText::t("The titleBar change takes effect after a restart.",
                      "titleBar の変更は再起動後に適用されます。"),
            6000);
    } else if (m_config.warningText().isEmpty()) {
        statusBar()->showMessage(UiText::t("Configuration reloaded.", "設定を再読み込みしました。"), 3500);
    }
}

// Shows every config.toml problem in a dialog (each entry carries its
// config.toml line number) plus the first line in the status bar. The app
// keeps running on the previous / default values.
void MainWindow::showConfigWarnings()
{
    const QString warnings = m_config.warningText();
    if (warnings.isEmpty()) {
        return;
    }
    statusBar()->showMessage(warnings.section('\n', 0, 0), 6000);
    auto *box = new QMessageBox(QMessageBox::Warning,
                                UiText::t("Configuration Errors", "設定エラー"),
                                UiText::t("config.toml has problems; the previous or default values are used:\n\n%1",
                                          "config.toml に問題があります。直前の値または既定値を使用します:\n\n%1")
                                    .arg(warnings),
                                QMessageBox::Ok, this);
    // Warnings embed config.toml content; never let Qt's rich-text
    // auto-detection interpret it as HTML.
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
}

// Opens config.toml in the configured editor ({path} expands to the file,
// environment variables are expanded), falling back to the OS association.
void MainWindow::openConfigInEditor()
{
    const QString configFile = AppConfig::configFilePath();
    QSettings settings;
    QString command = settings.value("Editor/command").toString().trimmed();
    if (!command.isEmpty()) {
        const bool hadPathToken = command.contains(QLatin1String("{path}"));
        command.replace(QLatin1String("{path}"), configFile);
        static const QRegularExpression envPattern(R"(\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?)");
        QRegularExpressionMatchIterator it = envPattern.globalMatch(command);
        QString expanded;
        int last = 0;
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            expanded += command.mid(last, match.capturedStart() - last);
            expanded += qEnvironmentVariable(match.captured(1).toUtf8().constData());
            last = match.capturedEnd();
        }
        expanded += command.mid(last);
        QStringList parts = QProcess::splitCommand(expanded);
        if (!parts.isEmpty()) {
            const QString program = parts.takeFirst();
            if (!hadPathToken) {
                parts.append(configFile);
            }
            if (QProcess::startDetached(program, parts)) {
                return;
            }
        }
        statusBar()->showMessage(
            UiText::t("Could not start the configured editor; opening with the default application.",
                      "設定されたエディターを起動できないため、既定のアプリケーションで開きます。"),
            6000);
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(configFile));
}

void MainWindow::showEditorSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(UiText::t("Editor Settings", "エディター設定"));
    dialog.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&dialog);
    auto *explanation = new QLabel(
        UiText::t("Command used by \"Edit Config File...\". {path} expands to config.toml and "
                  "environment variables are expanded. Leave empty to use the system default "
                  "application.",
                  "「設定ファイルを編集...」で使うコマンドです。{path} は config.toml に展開され、"
                  "環境変数も展開されます。空欄の場合はシステム既定のアプリケーションで開きます。"),
        &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    auto *commandEdit = new QLineEdit(&dialog);
    QSettings settings;
    commandEdit->setText(settings.value("Editor/command").toString());
    commandEdit->setPlaceholderText(QStringLiteral("code --wait {path}"));
    layout->addWidget(commandEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        settings.setValue("Editor/command", commandEdit->text().trimmed());
    }
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
    QStringList history;
    for (int index = 0; index < m_searchEdit->count(); ++index) {
        history << m_searchEdit->itemText(index);
    }

    const QStringList updated = tfx::core::updatedSearchHistory(history, term);
    if (updated == history) {
        return;
    }

    m_searchEdit->clear();
    for (const QString &item : updated) {
        m_searchEdit->addItem(item);
    }
    m_searchEdit->setCurrentIndex(0);
    m_searchEdit->setEditText(updated.first());
    saveSettings();
}
