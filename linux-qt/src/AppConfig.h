#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct UserCommand
{
    QString name;
    QString command;
    QString shortcut;
    QString workingDirectory = "{cwd}";
    bool requiresSelection = true;
    bool showOutput = true;
};

struct AppColors
{
    QString appBackground = "#151A1E";
    QString panelBackground = "#151A1E";
    QString sidebarBackground = "#171C20";
    QString inputBackground = "#10161A";
    QString terminalBackground = "#050607";
    QString foreground = "#D9E1E8";
    QString directoryForeground = "#E5EDF3";
    QString secondaryForeground = "#9EABB6";
    QString headerForeground = "#B9C4CC";
    QString selectedBackground = "#31576B";
    QString selectedForeground = "#FFFFFF";
    QString border = "#2A333A";
    QString activeBorder = "#36E67A";
    QString activeAccent = "#63F28D";
    QString hoverBackground = "#1F2830";
    QString disabledForeground = "#6D7881";
    QString scrollbarThumb = "#3A454E";
    QString scrollbarThumbHovered = "#4A5963";
    QString scrollbarThumbDragging = "#5C7484";
};

struct AppFontConfig
{
    QString ui = "system";
    QString mono = "monospace";
    int size = 12;

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
};

struct AppNamingConfig
{
    QString placeholderLanguage = "auto";
};

struct AppOpacityConfig
{
    double background = 1.0;
    double inactivePane = 1.0;
    double disabledItem = 1.0;
};

class AppConfig
{
public:
    static AppConfig loadOrCreate();

    QString shortcut(const QString &name, const QString &fallback) const;
    QString resolvedUiFontFamily() const;
    QString resolvedMonoFontFamily() const;
    QString warningText() const;

    AppColors colors;
    AppFontConfig font;
    AppStartupConfig startup;
    AppNamingConfig naming;
    AppOpacityConfig opacity;
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
    void applyValue(const QString &section, const QString &key, const QString &value, int lineNumber);
    void validateShortcutConflicts();
    void addWarning(int lineNumber, const QString &message);

    QStringList m_warnings;
};
