#include "FilePane.h"
#include "UiText.h"
#include "core/TabState.h"

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QMenu>
#include <QTabBar>
#include <QtGlobal>

using namespace tfx::core;

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
