#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct UserCommand
{
    QString name;
    QString run;                    // command line or multi-line script body
    QString shortcut;
    QStringList extensions;         // lowercase, no dot; empty or {"*"} = all
    QString target = "any";         // file | folder | current | any
    QString selection = "any";      // single | multiple | any
    bool requireGit = false;        // show only inside a Git work tree
    bool terminal = false;          // stream output to the terminal Output tab
    QString shell;                  // empty => $SHELL, then /bin/sh
    int shortcutLine = 0;           // config.toml line of the shortcut key (0 = none)

    // Legacy fields kept until the execution model adopts the fields above.
    QString workingDirectory = "{cwd}";
    bool requiresSelection = true;
    bool showOutput = true;
};

struct AppColors
{
    // Palette modelled on prism-fm: dark blue-grey surfaces meant to be seen
    // through, neutral greys for selection, and text as tints of white.
    QString appBackground = "#1E1E28";
    QString panelBackground = "#1E1E28";
    QString sidebarBackground = "#2D2D3C";
    QString inputBackground = "#3C3C50";
    QString terminalBackground = "#14141C";
    QString foreground = "#F2F2F2";
    QString directoryForeground = "#FFFFFF";
    QString secondaryForeground = "#B3B3B3";
    QString headerForeground = "#8C8C99";
    QString selectedBackground = "#646478";
    QString selectedForeground = "#FFFFFF";
    QString border = "#45454F";
    QString activeBorder = "#AAAAAA";
    QString activeAccent = "#AAAAAA";
    QString hoverBackground = "#4B4B5A";

    // Drop target
    QString dropTargetBackground = "#64FF96";

    // Pane title bar
    QString titleBarActive = "#3C3C50";
    QString titleBarInactive = "#1E1E28";

    // Status line
    QString statusBackground = "#1E1E28";
    QString statusForegroundActive = "#F2F2F2";
    QString statusForegroundInactive = "#8C8C99";

    // Folder tree
    QString folderTreeForeground = "#B3B3B3";
    QString folderTreeSelectedForeground = "#FFFFFF";
    QString folderTreeSelectedActive = "#646478";
    QString folderTreeSelectedInactive = "#3C3C50";
    QString folderTreeSectionHeader = "#8C8C99";

    // Split handle
    QString splitHandleIdle = "#2D2D3C";

    // Git status badges
    QString gitModified = "#E2C08D";
    QString gitAdded = "#7FB37F";
    QString gitDeleted = "#D48A8A";
    QString gitRenamed = "#8DB0E2";
    QString gitUntracked = "#8AC7A0";
    QString gitIgnored = "#8C8C99";
    QString gitConflicted = "#E28D8D";
};

struct AppThemeConfig
{
    QString name = "dark";
};

struct AppFontConfig
{
    QString ui = "system";
    QString mono = "monospace";
    int size = 13;

    // Optional per-pane overrides. Empty family / size 0 means inherit the
    // global mono family / size above.
    QString fileListFamily;
    int fileListSize = 0;
    QString previewFamily;
    int previewSize = 0;
    QString terminalFamily;
    int terminalSize = 0;
    QString folderTreeFamily;
    int folderTreeSize = 0;
};

struct AppStartupConfig
{
    QString layout = "restore";
    QString preview = "restore";
    QString terminal = "restore";
    QString folderTree = "restore";
    QString geometry;
    QString rightFolder;
    QStringList rightFolders;

    // First existing folder in `rightFolders`, else `rightFolder` when it is a
    // directory. Empty when nothing is configured or nothing resolves, which
    // leaves the right pane on its restored session path.
    QString resolvedRightFolder() const;
};

struct AppNamingConfig
{
    QString placeholderLanguage = "auto";
};

struct AppWindowConfig
{
    // "system" keeps the native title bar; "integrated" hides it and merges
    // the window controls into the menu bar. Applied at launch only.
    QString titleBar = "system";
};

struct AppOpacityConfig
{
    double background = 0.65;
    double inactivePane = 1.0;
    double disabledItem = 1.0;
    double headerSecondary = 0.75;
    double selectedParentRow = 0.8;
    double dropIndicator = 0.85;
    double dragPreview = 0.96;
    double dragPreviewShadow = 0.18;
    double subtleBackground = 0.07;
};

struct AppPreviewConfig
{
    QString defaultMode = "auto";                 // auto | rendered | text | none
    QHash<QString, QString> extensionModes;       // lowercase ext -> mode
    QString markdownExternalImages = "button";    // button | always | never
};

class AppConfig
{
public:
    static AppConfig loadOrCreate();
    // Absolute path of config.toml (for in-app editing and the live-reload watcher).
    static QString configFilePath();

    QString shortcut(const QString &name, const QString &fallback) const;
    QString resolvedUiFontFamily() const;
    QString resolvedMonoFontFamily() const;
    QString warningText() const;

    AppColors colors;
    AppThemeConfig theme;
    AppFontConfig font;
    AppStartupConfig startup;
    AppNamingConfig naming;
    AppWindowConfig window;
    AppOpacityConfig opacity;
    AppPreviewConfig preview;
    QHash<QString, QString> shortcuts;
    QHash<QString, QString> openWith;
    QList<UserCommand> commands;
    QString terminalApp;
    QString terminalArguments;
    QString terminalShell;
    QString terminalColorScheme;

private:
    static QString configDirectory();
    static QString configPath();
    static QString defaultConfigText();
    static QString stripComment(const QString &line);
    static QString unquote(const QString &value, bool *ok);
    static QStringList parseStringArray(const QString &value, bool *ok);
    static QString expandPath(QString path);
    static bool isColor(const QString &value);
    void applyThemePreset(const QString &name);
    void applyValue(const QString &section, const QString &key, const QString &value, int lineNumber);
    void validateShortcutConflicts();
    void addWarning(int lineNumber, const QString &message);

    QStringList m_warnings;
    // config.toml line of each [shortcuts] assignment, for conflict warnings.
    QHash<QString, int> m_shortcutLines;
};
