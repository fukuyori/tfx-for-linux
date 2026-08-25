#include "MainWindow.h"
#include "views/SidebarViews.h"

#include <QApplication>

namespace {
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
}

// Applies the whole theme: the application stylesheet plus the widget-side
// settings (fonts, pane colors, terminal scheme) that a stylesheet cannot
// express. Safe to call repeatedly — the config live reload re-runs it.
void MainWindow::applyTerminalTheme()
{
    qApp->setStyleSheet(buildThemeStyleSheet());
    applyPaneThemeSettings();
}

// Assembles the application stylesheet from the dark-theme template: the
// hardcoded palette below is substituted with the [colors]/[opacity]/[font]
// values, then the per-pane font and color rules are appended.
QString MainWindow::buildThemeStyleSheet() const
{
    QString styleSheet = R"(
        QMainWindow, QWidget {
            background: #151A1E;
            color: #D9E1E8;
            font-family: __UI_FONT__;
            font-size: 12px;
        }
        QMenuBar { background: #11161A; color: #D9E1E8; border: 0; }
        QMenuBar::item { padding: 5px 10px; border-radius: 6px; }
        QMenuBar::item:selected { background: #1F2830; }
        QMenu {
            background: #11161A;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 8px;
            padding: 6px;
        }
        QMenu::item {
            padding: 7px 24px 7px 20px;
            border-radius: 6px;
        }
        QMenu::item:selected { background: #243947; color: #FFFFFF; }
        QMenu::separator {
            height: 1px;
            background: #2A333A;
            margin: 6px 10px;
        }
        QToolBar#topToolbar {
            background: #151A1E;
            border: 0;
            padding: 7px 10px;
            spacing: 7px;
        }
        QLineEdit#pathEdit {
            background: #0F1418;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 8px;
            padding: 7px 12px;
            font-weight: 500;
        }
        QComboBox#searchEdit,
        QComboBox#searchEdit QLineEdit {
            background: #10161A;
            color: #D9E1E8;
            border: 1px solid #2A333A;
            border-radius: 8px;
            padding: 7px 11px;
            selection-background-color: #243947;
        }
        QComboBox#searchEdit::drop-down {
            border: 0;
            width: 18px;
        }
        /* Dock title rows have no style of their own, so with a translucent
           window they painted nothing at all and showed as holes. Giving them
           the window-chrome colour puts them at the configured opacity like
           the menu bar. */
        QDockWidget {
            color: #9EABB6;
            border: 0;
        }
        QDockWidget::title {
            background: #11161A;
            padding: 6px 10px;
            border: 0;
            text-align: left;
        }
        QDockWidget::close-button, QDockWidget::float-button {
            background: transparent;
            border: 0;
            border-radius: 4px;
            padding: 1px;
        }
        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background: #1F2830;
        }
        QWidget#sidebar {
            background: #171C20;
            border: 0;
            padding: 4px;
        }
        /* The sidebar owns its surface; its views and their scroll viewports
           would otherwise paint a second layer over it (see the note on
           QTableView#fileTable). */
        QTreeView#folderTree,
        QListWidget#pinnedList,
        QListWidget#diskList,
        QTreeView#folderTree > QWidget,
        QListWidget#pinnedList > QWidget,
        QListWidget#diskList > QWidget {
            background: transparent;
        }
        QDialog#sortOptionsDialog {
            background: #11161A;
            border: 1px solid #2A333A;
        }
        QLabel#sortOptionsTitle {
            color: #63F28D;
            font-weight: 700;
            padding-bottom: 2px;
        }
        QListWidget#sortOptionsList {
            background: transparent;
            color: #9EABB6;
            outline: 0;
        }
        QListWidget#sortOptionsList::item {
            padding: 3px 4px;
            border: 0;
        }
        QListWidget#sortOptionsList::item:selected {
            background: transparent;
            color: #63F28D;
        }
        QLabel#sortOptionsHint {
            color: #9EABB6;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: transparent;
            border: 0;
            margin: 0;
        }
        QScrollBar:vertical { width: 10px; }
        QScrollBar:horizontal { height: 10px; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #2A333A;
            border-radius: 5px;
            min-height: 28px;
            min-width: 28px;
        }
        QScrollBar::handle:hover { background: #1F2830; }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0; height: 0; border: 0; background: transparent;
        }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QLabel#sectionLabel {
            color: #9EABB6;
            font-size: 11px;
            font-weight: 600;
            padding: 14px 6px 6px 6px;
        }
        QToolButton#sectionHeader {
            color: #9EABB6;
            font-size: 11px;
            font-weight: 600;
            border: 0;
            background: transparent;
            padding: 14px 6px 6px 6px;
        }
        QListWidget, QTreeView {
            background: #171C20;
            color: #D9E1E8;
            selection-background-color: #263D4C;
            selection-color: #FFFFFF;
            border: 0;
            outline: 0;
        }
        QListWidget::item, QTreeView::item {
            border-radius: 6px;
            padding: 1px 4px;
        }
        QTableView#fileTable {
            /* Transparent, not the panel colour: QWidget#filePane already
               paints that colour underneath. Painting it again stacks a second
               translucent layer, and with [opacity] background < 1 the alphas
               compound until the rows behind the file names look opaque. */
            background: transparent;
            color: #D9E1E8;
            selection-background-color: #263D4C;
            selection-color: #FFFFFF;
            border: 0;
            outline: 0;
            gridline-color: #293137;
        }
        QTableView#fileTable::item {
            padding: 0px 6px;
            margin: 0;
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
        QHeaderView { background: transparent; border: 0; }
        QHeaderView::section {
            background: transparent;
            color: #B9C4CC;
            border: 0;
            border-bottom: 1px solid #2A333A;
            padding: 7px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QTabBar#paneTabs {
            background: transparent;
            border: 0;
            min-height: 26px;
        }
        QTabBar#paneTabs::tab {
            background: transparent;
            color: #9EABB6;
            border: 0;
            border-radius: 8px;
            padding: 4px 12px;
            margin: 3px 3px 3px 0;
        }
        QTabBar#paneTabs::tab:hover { background: #1F2830; }
        QTabBar#paneTabs::tab:selected {
            color: #D9E1E8;
            background: #10161A;
        }
        QWidget#filePane {
            background: #151A1E;
            border: 1px solid #2A333A;
            border-radius: 8px;
        }
        QWidget#filePane[activePane="true"] {
            border: 1px solid #36E67A;
        }
        /* Pure containers between the pane and its views: the pane owns the
           surface, so these must not repaint it (see QTableView#fileTable).
           The scroll-area viewports count too — they are plain QWidgets and
           the generic rule above would have them paint the surface again. */
        QWidget#filePane > QStackedWidget,
        QSplitter#filePaneSplitter,
        QTableView#fileTable > QWidget,
        QListView#fileIcons > QWidget {
            background: transparent;
        }
        QLabel#paneBadge {
            color: #AEBBC5;
            background: transparent;
            border: 1px solid #2A333A;
            border-radius: 6px;
            padding: 3px 10px;
            font-weight: 600;
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
        QToolButton#breadcrumbSegment {
            color: #AEBBC5;
            background: transparent;
            border: 0;
            padding: 0px 2px;
            font-weight: 600;
        }
        QToolButton#breadcrumbSegment:hover {
            color: #63F28D;
            text-decoration: underline;
        }
        QLabel#breadcrumbSeparator {
            color: #9EABB6;
            font-weight: 600;
        }
        QLabel#breadcrumbStatic {
            color: #AEBBC5;
            font-weight: 600;
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
            border-color: #2A333A;
        }
        QToolButton#toolbarIconButton:checked,
        QToolButton#previewSourceToggle:checked {
            background: #263D4C;
            border-color: #243947;
            color: #FFFFFF;
        }
        QSplitter::handle {
            background: #20272D;
        }
        QMainWindow::separator {
            background: #20272D;
            width: 7px;
            height: 7px;
        }
        QMainWindow::separator:hover {
            background: #243947;
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
            /* QWidget#previewPane paints the surface, so the views stay clear
               (see the note on QTableView#fileTable). */
            background: transparent;
            color: #D9E1E8;
            border: 0;
            selection-background-color: #263D4C;
        }
        /* Containers and scroll viewports inside the preview: the pane owns the
           surface, so these must not repaint it. */
        QWidget#previewPane QStackedWidget,
        QPlainTextEdit#previewCode > QWidget,
        QTextBrowser#previewRendered > QWidget {
            background: transparent;
        }
        /* The toolbar's stretch filler would otherwise paint the surface the
           toolbar itself already paints. */
        QWidget#toolbarSpacer {
            background: transparent;
        }
        QLabel#previewTitle {
            background: transparent;
            color: #9EABB6;
            font-weight: 600;
            padding: 4px 2px;
        }
        QWidget#terminalPane {
            background: #050607;
            border-top: 1px solid #2A333A;
        }
        /* The tab stack and its page are containers around the terminal
           widget; the pane above already paints the surface, and repainting it
           here would stack another translucent layer (see QTableView#fileTable). */
        QWidget#terminalPane QStackedWidget,
        QWidget#terminalPane QStackedWidget > QWidget {
            background: transparent;
        }
        QLabel#terminalTitle {
            color: #9EABB6;
            padding: 4px 6px;
            font-weight: 500;
        }
        QTabWidget#terminalTabs::pane {
            border: 0;
            border-top: 1px solid #2A333A;
        }
        QTabWidget#terminalTabs > QTabBar::tab {
            background: #10161A;
            color: #9EABB6;
            border: 1px solid #2A333A;
            border-bottom: 0;
            padding: 3px 12px;
            margin-right: 2px;
        }
        QTabWidget#terminalTabs > QTabBar::tab:selected {
            background: #050607;
            color: #D9E1E8;
        }
        QTabWidget#terminalTabs > QTabBar::tab:hover {
            color: #D9E1E8;
        }
        QToolButton#terminalActionButton {
            background: #10161A;
            border: 1px solid #2A333A;
            border-radius: 3px;
            color: #D9E1E8;
            padding: 2px 8px;
            margin: 2px 2px;
            min-height: 18px;
            font-weight: 600;
        }
        QToolButton#terminalActionButton:hover {
            background: #1F2830;
            border-color: #2A333A;
        }
        QToolButton#terminalActionButton:pressed {
            background: #243947;
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
    styleSheet.replace("#20272D", m_config.colors.splitHandleIdle);
    styleSheet.replace("__UI_FONT__", m_config.resolvedUiFontFamily());
    styleSheet.replace("__MONO_FONT__", m_config.resolvedMonoFontFamily());
    styleSheet.replace("font-size: 12px;", QString("font-size: %1px;").arg(m_config.font.size));
    if (m_config.opacity.disabledItem < 1.0) {
        styleSheet += QString(
            "\nQWidget:disabled, QToolButton:disabled, QPushButton:disabled,"
            " QMenu::item:disabled, QMenuBar::item:disabled {"
            " color: %1; }\n")
            .arg(withAlpha(m_config.colors.foreground, m_config.opacity.disabledItem));
    }

    const auto paneFontRule = [this](const QString &selector, const QString &family, int size,
                                     bool monospacedFallback = true) {
        const QString fallback = monospacedFallback ? m_config.resolvedMonoFontFamily()
                                                    : m_config.resolvedUiFontFamily();
        const QString resolvedFamily = family.isEmpty() ? fallback : family;
        const int resolvedSize = size > 0 ? size : m_config.font.size;
        return QString("\n%1 { font-family: %2; font-size: %3px; }\n")
            .arg(selector, resolvedFamily).arg(resolvedSize);
    };
    styleSheet += paneFontRule("QTableView#fileTable, QListView#fileIcons",
                               m_config.font.fileListFamily, m_config.font.fileListSize, false);
    styleSheet += paneFontRule("QPlainTextEdit#previewCode, QTextBrowser#previewRendered",
                               m_config.font.previewFamily, m_config.font.previewSize);
    styleSheet += paneFontRule("QTreeView#folderTree, QListWidget#pinnedList, QListWidget#diskList",
                               m_config.font.folderTreeFamily, m_config.font.folderTreeSize, false);

    // Folder-tree, section-header, and status-line colours (config [colors]).
    styleSheet += QString(
        "\nQTreeView#folderTree { color: %1; }"
        "\nQTreeView#folderTree::item:selected:active,"
        " QListWidget#pinnedList::item:selected:active { background: %2; color: %3; }"
        "\nQTreeView#folderTree::item:selected:!active,"
        " QListWidget#pinnedList::item:selected:!active { background: %8; color: %3; }"
        "\nQLabel#sectionLabel { color: %4; }"
        "\nQToolButton#sectionHeader { color: %4; }"
        "\nQLabel#paneStatus { background: %5; color: %6; }"
        "\nQLabel#paneStatus[activePane=\"true\"] { color: %7; }"
        "\nQWidget#paneTitleBar { background: %9; }"
        "\nQWidget#paneTitleBar[activePane=\"true\"] { background: %10; }"
        // The generic QWidget background rule also matches the header's path
        // container, and Qt then paints it over the title bar; keep it clear
        // so the configured title-bar colour is the one that shows.
        "\nQStackedWidget#panePathStack { background: transparent; }\n")
        .arg(m_config.colors.folderTreeForeground,
             m_config.colors.folderTreeSelectedActive,
             m_config.colors.folderTreeSelectedForeground,
             m_config.colors.folderTreeSectionHeader,
             m_config.colors.statusBackground,
             m_config.colors.statusForegroundInactive,
             m_config.colors.statusForegroundActive,
             m_config.colors.folderTreeSelectedInactive)
        .arg(m_config.colors.titleBarInactive)
        .arg(m_config.colors.titleBarActive);

    return styleSheet;
}

// Widget-side theme settings that a stylesheet cannot carry: pane fonts (the
// file-list font is also set as a widget font so the item views' elision
// metrics match the painted glyphs), terminal font and color scheme, Git
// badge colors, and drop-indicator colors.
void MainWindow::applyPaneThemeSettings()
{
    // Must match the family the stylesheet gives the file list, or the elision
    // metrics below are measured with a different face than the one painted.
    const QString family = m_config.font.fileListFamily.isEmpty()
        ? m_config.resolvedUiFontFamily()
        : m_config.font.fileListFamily;
    const int pixelSize = m_config.font.fileListSize > 0
        ? m_config.font.fileListSize
        : m_config.font.size;
    QFont fileListFont(family);
    fileListFont.setPixelSize(pixelSize);
    m_leftPane->setFileListFont(fileListFont);
    m_rightPane->setFileListFont(fileListFont);

    m_terminalPane->setBackgroundOpacity(m_config.opacity.background);
    m_terminalPane->setContentFont(TerminalPane::resolveFont(
        m_config.font.terminalFamily,
        m_config.font.terminalSize > 0 ? m_config.font.terminalSize : m_config.font.size));
    m_terminalPane->setColorScheme(m_config.terminalColorScheme);
    m_leftPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);
    m_rightPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);
    for (FilePane *pane : {m_leftPane, m_rightPane}) {
        pane->setRowColors(m_config.colors.selectedBackground, m_config.colors.hoverBackground,
                           m_config.colors.selectedForeground);
    }

    // Git status badge colours, keyed by the single-letter porcelain label.
    const QHash<QString, QString> gitColors = {
        {"M", m_config.colors.gitModified},
        {"A", m_config.colors.gitAdded},
        {"D", m_config.colors.gitDeleted},
        {"R", m_config.colors.gitRenamed},
        {"C", m_config.colors.gitRenamed},
        {"?", m_config.colors.gitUntracked},
        {"!", m_config.colors.gitIgnored},
        {"U", m_config.colors.gitConflicted},
    };
    m_leftPane->setGitStatusColors(gitColors);
    m_rightPane->setGitStatusColors(gitColors);

    m_leftPane->setDropTargetColor(m_config.colors.dropTargetBackground);
    m_rightPane->setDropTargetColor(m_config.colors.dropTargetBackground);
    static_cast<FolderTreeView *>(m_treeView)->dropTargetColor =
        QColor(m_config.colors.dropTargetBackground);

    static_cast<PinnedListWidget *>(m_pinnedList)->dropIndicatorOpacity = m_config.opacity.dropIndicator;
    static_cast<PinnedListWidget *>(m_pinnedList)->dropTargetColor =
        QColor(m_config.colors.dropTargetBackground);
    m_pinnedList->viewport()->update();

    m_previewPane->setPreviewConfig(m_config.preview.defaultMode,
                                    m_config.preview.extensionModes,
                                    m_config.preview.markdownExternalImages);
}
