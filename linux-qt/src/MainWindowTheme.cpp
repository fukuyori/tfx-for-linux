#include "MainWindow.h"
#include "MainWindowSidebar.h"

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
        QMenu::separator {
            height: 1px;
            background: #4A5963;
            margin: 5px 8px;
        }
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
        QToolButton#sectionHeader {
            color: #9EABB6;
            font-weight: 500;
            border: 0;
            background: transparent;
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
            color: #5B6770;
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
            background: #1B232A;
            border: 1px solid #37434C;
            border-radius: 3px;
            color: #D9E1E8;
            padding: 2px 8px;
            margin: 2px 2px;
            min-height: 18px;
            font-weight: 600;
        }
        QToolButton#terminalActionButton:hover {
            background: #26313A;
            border-color: #4A5963;
        }
        QToolButton#terminalActionButton:pressed {
            background: #304049;
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
    styleSheet.replace("__MONO_FONT__", m_config.resolvedMonoFontFamily());
    styleSheet.replace("font-size: 12px;", QString("font-size: %1px;").arg(m_config.font.size));
    if (m_config.opacity.disabledItem < 1.0) {
        styleSheet += QString(
            "\nQWidget:disabled, QToolButton:disabled, QPushButton:disabled,"
            " QMenu::item:disabled, QMenuBar::item:disabled {"
            " color: %1; }\n")
            .arg(withAlpha(m_config.colors.foreground, m_config.opacity.disabledItem));
    }

    const auto paneFontRule = [this](const QString &selector, const QString &family, int size) {
        const QString resolvedFamily = family.isEmpty() ? m_config.resolvedMonoFontFamily() : family;
        const int resolvedSize = size > 0 ? size : m_config.font.size;
        return QString("\n%1 { font-family: %2; font-size: %3px; }\n")
            .arg(selector, resolvedFamily).arg(resolvedSize);
    };
    // The file-list font is also applied as a widget font: when only the
    // stylesheet sets it, the item views elide text with the widget font's
    // metrics while painting with the stylesheet font, over-shortening long
    // names. Setting the identical font on the widgets keeps the metrics and
    // the painted glyphs in sync.
    {
        const QString family = m_config.font.fileListFamily.isEmpty()
            ? m_config.resolvedMonoFontFamily()
            : m_config.font.fileListFamily;
        const int pixelSize = m_config.font.fileListSize > 0
            ? m_config.font.fileListSize
            : m_config.font.size;
        QFont fileListFont(family);
        fileListFont.setPixelSize(pixelSize);
        m_leftPane->setFileListFont(fileListFont);
        m_rightPane->setFileListFont(fileListFont);
    }
    styleSheet += paneFontRule("QTableView#fileTable, QListView#fileIcons",
                               m_config.font.fileListFamily, m_config.font.fileListSize);
    styleSheet += paneFontRule("QPlainTextEdit#previewCode, QTextBrowser#previewRendered",
                               m_config.font.previewFamily, m_config.font.previewSize);
    styleSheet += paneFontRule("QTreeView#folderTree, QListWidget#pinnedList, QListWidget#diskList",
                               m_config.font.folderTreeFamily, m_config.font.folderTreeSize);

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
        "\nQWidget#paneTitleBar[activePane=\"true\"] { background: %10; }\n")
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

    qApp->setStyleSheet(styleSheet);

    m_terminalPane->setContentFont(TerminalPane::resolveFont(
        m_config.font.terminalFamily,
        m_config.font.terminalSize > 0 ? m_config.font.terminalSize : m_config.font.size));
    m_terminalPane->setColorScheme(m_config.terminalColorScheme);
    m_leftPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);
    m_rightPane->setThemeColors(m_config.colors.foreground, m_config.colors.directoryForeground);

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
    m_pinnedList->viewport()->update();

    m_previewPane->setPreviewConfig(m_config.preview.defaultMode,
                                    m_config.preview.extensionModes,
                                    m_config.preview.markdownExternalImages);
}
