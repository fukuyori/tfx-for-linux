#include "MainWindow.h"
#include "UiText.h"
#include "core/SearchState.h"

#include <QComboBox>
#include <QDockWidget>
#include <QFileInfo>
#include <QListWidget>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QToolButton>

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
    settings.setValue("MainWindow/state", saveState(1));
    settings.setValue("MainWindow/paneSplitter", m_paneSplitter->saveState());
    settings.setValue("View/sidebarVisible", m_dockSidebar->isVisible());
    settings.setValue("View/splitVisible", !m_rightPane->isHidden());
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
