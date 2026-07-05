#include "MainWindow.h"

#include <QDir>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

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
    if (m_sidebarButton && m_sidebarButton->isChecked() != visible) {
        const QSignalBlocker blocker(m_sidebarButton);
        m_sidebarButton->setChecked(visible);
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
    if (m_commandOutputButton && m_commandOutputButton->isChecked() != visible) {
        const QSignalBlocker blocker(m_commandOutputButton);
        m_commandOutputButton->setChecked(visible);
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
    if (m_hiddenButton && m_hiddenButton->isChecked() != visible) {
        m_hiddenButton->setChecked(visible);
    }
    if (!m_isRestoringSettings) {
        saveSettings();
    }
}
