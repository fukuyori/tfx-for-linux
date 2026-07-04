#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QString normalizedShortcut(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty()) {
        return {};
    }

    const QStringList parts = value.split('+', Qt::SkipEmptyParts);
    QStringList normalized;
    for (QString part : parts) {
        part = part.trimmed();
        if (part == "ctrl" || part == "control") {
            normalized << "Ctrl";
        } else if (part == "shift") {
            normalized << "Shift";
        } else if (part == "alt") {
            normalized << "Alt";
        } else if (part == "backslash") {
            normalized << "\\";
        } else if (part == "delete") {
            normalized << "Del";
        } else if (part == "escape") {
            normalized << "Esc";
        } else if (part == "return") {
            normalized << "Enter";
        } else if (part == "up" || part == "down" || part == "left" || part == "right") {
            normalized << part.left(1).toUpper() + part.mid(1);
        } else if (part.size() == 1) {
            normalized << part.toUpper();
        } else if (part.startsWith('f') && part.size() <= 3) {
            normalized << part.toUpper();
        } else {
            normalized << part;
        }
    }
    return normalized.join('+');
}

QHash<QString, QString> defaultShortcuts()
{
    return {
        {"reload", normalizedShortcut("f5")},
        {"openTerminal", normalizedShortcut("ctrl+shift+t")},
        {"togglePreview", normalizedShortcut("ctrl+shift+p")},
        {"togglePreviewSource", normalizedShortcut("ctrl+shift+r")},
        {"openPreviewExternal", normalizedShortcut("ctrl+shift+i")},
        {"toggleSplit", normalizedShortcut("ctrl+backslash")},
        {"focusSearch", normalizedShortcut("ctrl+f")},
        {"toggleHidden", normalizedShortcut("ctrl+shift+.")},
        {"goBack", normalizedShortcut("alt+left")},
        {"goForward", normalizedShortcut("alt+right")},
        {"goUp", normalizedShortcut("alt+up")},
        {"newFolder", normalizedShortcut("ctrl+shift+n")},
        {"newFile", normalizedShortcut("ctrl+n")},
        {"rename", normalizedShortcut("f2")},
        {"moveToTrash", normalizedShortcut("delete")},
        {"copyItems", normalizedShortcut("ctrl+c")},
        {"cutItems", normalizedShortcut("ctrl+x")},
        {"pasteItems", normalizedShortcut("ctrl+v")},
        {"newTab", normalizedShortcut("ctrl+t")},
        {"closeTab", normalizedShortcut("ctrl+w")},
        {"prevTab", normalizedShortcut("ctrl+shift+[")},
        {"nextTab", normalizedShortcut("ctrl+shift+]")},
        {"toggleTerminal", normalizedShortcut("ctrl+j")},
        {"quit", normalizedShortcut("ctrl+q")},
    };
}
}

AppConfig AppConfig::loadOrCreate()
{
    AppConfig config;
    QDir().mkpath(configDirectory());

    QFile file(configPath());
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << defaultConfigText();
            file.close();
        }
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        config.addWarning(0, QString("Cannot read config.toml: %1").arg(file.errorString()));
        return config;
    }

    QString section;
    int lineNumber = 0;
    while (!file.atEnd()) {
        ++lineNumber;
        const QString rawLine = QString::fromUtf8(file.readLine());
        const QString line = stripComment(rawLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line == "[[commands]]") {
            config.commands.append(UserCommand());
            section = "commands";
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }

        const int equals = line.indexOf('=');
        if (equals <= 0) {
            config.addWarning(lineNumber, "Invalid assignment syntax");
            continue;
        }
        config.applyValue(section, line.left(equals).trimmed(), line.mid(equals + 1).trimmed(), lineNumber);
    }
    config.validateShortcutConflicts();
    return config;
}

QString AppConfig::shortcut(const QString &name, const QString &fallback) const
{
    return shortcuts.value(name, fallback);
}

QString AppConfig::resolvedUiFontFamily() const
{
    if (font.ui == "system") {
        return "Noto Sans, Yu Gothic UI, Meiryo, sans-serif";
    }
    return font.ui;
}

QString AppConfig::resolvedMonoFontFamily() const
{
    if (font.mono == "monospace") {
        return "JetBrains Mono, Fira Code, Noto Sans Mono CJK JP, DejaVu Sans Mono, monospace";
    }
    return font.mono;
}

QString AppConfig::warningText() const
{
    return m_warnings.join('\n');
}

QString AppConfig::configDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("tfx");
}

QString AppConfig::configPath()
{
    return QDir(configDirectory()).filePath("config.toml");
}

QString AppConfig::defaultConfigText()
{
    return QStringLiteral(
        "version = 1\n"
        "\n"
        "[font]\n"
        "ui = \"system\"\n"
        "mono = \"monospace\"\n"
        "size = 12\n"
        "# Optional per-pane overrides (family and/or size):\n"
        "# fileList = \"monospace\"\n"
        "# fileListSize = 12\n"
        "# preview = \"monospace\"\n"
        "# previewSize = 12\n"
        "# terminal = \"monospace\"\n"
        "# terminalSize = 12\n"
        "# folderTree = \"monospace\"\n"
        "# folderTreeSize = 12\n"
        "\n"
        "[shortcuts]\n"
        "reload = \"f5\"\n"
        "openTerminal = \"ctrl+shift+t\"\n"
        "togglePreview = \"ctrl+shift+p\"\n"
        "togglePreviewSource = \"ctrl+shift+r\"\n"
        "openPreviewExternal = \"ctrl+shift+i\"\n"
        "toggleSplit = \"ctrl+backslash\"\n"
        "focusSearch = \"ctrl+f\"\n"
        "toggleHidden = \"ctrl+shift+.\"\n"
        "goBack = \"alt+left\"\n"
        "goForward = \"alt+right\"\n"
        "goUp = \"alt+up\"\n"
        "newFolder = \"ctrl+shift+n\"\n"
        "newFile = \"ctrl+n\"\n"
        "rename = \"f2\"\n"
        "moveToTrash = \"delete\"\n"
        "copyItems = \"ctrl+c\"\n"
        "cutItems = \"ctrl+x\"\n"
        "pasteItems = \"ctrl+v\"\n"
        "newTab = \"ctrl+t\"\n"
        "closeTab = \"ctrl+w\"\n"
        "prevTab = \"ctrl+shift+[\"\n"
        "nextTab = \"ctrl+shift+]\"\n"
        "toggleTerminal = \"ctrl+j\"\n"
        "quit = \"ctrl+q\"\n"
        "\n"
        "# [startup]\n"
        "# layout = \"restore\"\n"
        "# preview = \"restore\"\n"
        "# terminal = \"restore\"\n"
        "# folderTree = \"restore\"\n"
        "# rightFolder = \"~/Downloads\"\n"
        "# rightFolders = [\"~/Downloads\", \"~/Documents\"]\n"
        "\n"
        "# [opacity]\n"
        "# background = 0.40     # ウィンドウ背景の不透明度 (0.0=透明, 1.0=不透明)\n"
        "# inactivePane = 0.5    # 非アクティブペインの不透明度\n"
        "# disabledItem = 0.45   # 無効な項目の不透明度\n"
        "\n"
        "# [colors]\n"
        "# fileListBackground = \"#151A1E\"\n"
        "# fileForeground = \"#D9E1E8\"\n"
        "# directoryForeground = \"#E5EDF3\"\n"
        "# paneBorderKeyboardTarget = \"#36E67A\"\n"
        "\n"
        "# [terminal]\n"
        "# colorScheme = \"DarkPastels\"   # built-in QTermWidget scheme (sets background/foreground)\n"
        "# app = \"x-terminal-emulator\"\n"
        "# arguments = \"--working-directory={path}\"\n"
        "\n"
        "# [openWith]\n"
        "# md = \"code\"\n"
        "\n"
        "# [[commands]]\n"
        "# name = \"Open in VS Code\"\n"
        "# command = \"code {paths}\"\n"
        "# shortcut = \"ctrl+shift+o\"\n"
        "# requiresSelection = true\n"
        "# showOutput = false\n");
}

QString AppConfig::stripComment(const QString &line)
{
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        if (line.at(i) == '"' && (i == 0 || line.at(i - 1) != '\\')) {
            quoted = !quoted;
        } else if (line.at(i) == '#' && !quoted) {
            return line.left(i);
        }
    }
    return line;
}

QString AppConfig::unquote(const QString &value, bool *ok)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() < 2 || !trimmed.startsWith('"') || !trimmed.endsWith('"')) {
        *ok = false;
        return {};
    }
    QString text = trimmed.mid(1, trimmed.size() - 2);
    text.replace("\\\"", "\"");
    text.replace("\\\\", "\\");
    *ok = true;
    return text;
}

QStringList AppConfig::parseStringArray(const QString &value, bool *ok)
{
    QString trimmed = value.trimmed();
    if (!trimmed.startsWith('[') || !trimmed.endsWith(']')) {
        *ok = false;
        return {};
    }
    trimmed = trimmed.mid(1, trimmed.size() - 2).trimmed();
    if (trimmed.isEmpty()) {
        *ok = true;
        return {};
    }

    QStringList values;
    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool itemOk = false;
        const QString text = unquote(part.trimmed(), &itemOk);
        if (!itemOk) {
            *ok = false;
            return {};
        }
        values << expandPath(text);
    }
    *ok = true;
    return values;
}

QString AppConfig::expandPath(QString path)
{
    if (path == "~") {
        return QDir::homePath();
    }
    if (path.startsWith("~/")) {
        return QDir::home().filePath(path.mid(2));
    }
    return path;
}

bool AppConfig::isColor(const QString &value)
{
    static const QRegularExpression colorPattern("^#[0-9a-fA-F]{6}$");
    return colorPattern.match(value).hasMatch();
}

void AppConfig::applyValue(const QString &section, const QString &key, const QString &value, int lineNumber)
{
    if (section.isEmpty() && key == "version") {
        if (value != "1") {
            addWarning(lineNumber, "Unsupported config version");
        }
        return;
    }

    if (section == "font") {
        // Size keys: global "size" and per-pane "<area>Size".
        int *sizeTarget = nullptr;
        if (key == "size") sizeTarget = &font.size;
        else if (key == "fileListSize") sizeTarget = &font.fileListSize;
        else if (key == "previewSize") sizeTarget = &font.previewSize;
        else if (key == "terminalSize") sizeTarget = &font.terminalSize;
        else if (key == "folderTreeSize") sizeTarget = &font.folderTreeSize;
        if (sizeTarget) {
            bool ok = false;
            const int size = value.toInt(&ok);
            if (ok && size >= 8 && size <= 40) {
                *sizeTarget = size;
            } else {
                addWarning(lineNumber, "Invalid font size");
            }
            return;
        }
        // Family keys: global "ui"/"mono" and per-pane "<area>".
        QString *familyTarget = nullptr;
        if (key == "ui") familyTarget = &font.ui;
        else if (key == "mono") familyTarget = &font.mono;
        else if (key == "fileList") familyTarget = &font.fileListFamily;
        else if (key == "preview") familyTarget = &font.previewFamily;
        else if (key == "terminal") familyTarget = &font.terminalFamily;
        else if (key == "folderTree") familyTarget = &font.folderTreeFamily;
        if (familyTarget) {
            bool ok = false;
            const QString text = unquote(value, &ok);
            if (ok) {
                *familyTarget = text;
            } else {
                addWarning(lineNumber, "Invalid font value");
            }
            return;
        }
    }

    if (section == "colors") {
        bool ok = false;
        const QString color = unquote(value, &ok);
        if (!ok || !isColor(color)) {
            addWarning(lineNumber, QString("Invalid color value for %1").arg(key));
            return;
        }
        if (key == "fileListBackground") colors.panelBackground = color;
        else if (key == "headerBackground") colors.appBackground = color;
        else if (key == "inputBackground") colors.inputBackground = color;
        else if (key == "fileListRowHovered") colors.hoverBackground = color;
        else if (key == "fileListRowSelected") colors.selectedBackground = color;
        else if (key == "fileListRowSelectedForeground") colors.selectedForeground = color;
        else if (key == "fileForeground") colors.foreground = color;
        else if (key == "directoryForeground") colors.directoryForeground = color;
        else if (key == "secondaryForeground") colors.secondaryForeground = color;
        else if (key == "headerForeground") colors.headerForeground = color;
        else if (key == "paneBorderInactive") colors.border = color;
        else if (key == "paneBorderActive" || key == "paneBorderKeyboardTarget") colors.activeBorder = color;
        else if (key == "splitHandleActive") colors.activeAccent = color;
        else if (key == "folderTreeBackground") colors.sidebarBackground = color;
        else if (key == "disabledForeground") colors.disabledForeground = color;
        else if (key == "scrollbarThumb") colors.scrollbarThumb = color;
        else if (key == "scrollbarThumbHovered") colors.scrollbarThumbHovered = color;
        else if (key == "scrollbarThumbDragging") colors.scrollbarThumbDragging = color;
        return;
    }

    if (section == "opacity") {
        bool ok = false;
        double level = value.toDouble(&ok);
        if (!ok || level < 0.0 || level > 1.0) {
            addWarning(lineNumber, QString("Invalid opacity value for %1 (expected 0.0-1.0)").arg(key));
            return;
        }
        if (key == "background") opacity.background = level;
        else if (key == "inactivePane") opacity.inactivePane = level;
        else if (key == "disabledItem") opacity.disabledItem = level;
        else addWarning(lineNumber, QString("Unknown opacity key: %1").arg(key));
        return;
    }

    if (section == "startup") {
        bool ok = false;
        if (key == "layout" || key == "preview" || key == "terminal" || key == "folderTree" || key == "rightFolder") {
            const QString text = unquote(value, &ok);
            if (!ok) {
                addWarning(lineNumber, "Invalid startup value");
                return;
            }
            if (key == "layout" && (text == "single" || text == "split" || text == "restore")) startup.layout = text;
            else if (key == "preview" && (text == "show" || text == "hide" || text == "restore")) startup.preview = text;
            else if (key == "terminal" && (text == "show" || text == "hide" || text == "restore")) startup.terminal = text;
            else if (key == "folderTree" && (text == "show" || text == "hide" || text == "restore")) startup.folderTree = text;
            else if (key == "rightFolder") startup.rightFolder = expandPath(text);
            return;
        }
        if (key == "rightFolders") {
            startup.rightFolders = parseStringArray(value, &ok);
            if (!ok) {
                addWarning(lineNumber, "Invalid rightFolders value");
            }
            return;
        }
    }

    if (section == "shortcuts") {
        bool ok = false;
        const QString text = unquote(value, &ok);
        if (ok) {
            shortcuts.insert(key, normalizedShortcut(text));
        } else {
            addWarning(lineNumber, "Invalid shortcut value");
        }
        return;
    }

    if (section == "terminal") {
        bool ok = false;
        const QString text = unquote(value, &ok);
        if (!ok) {
            return;
        }
        if (key == "app") terminalApp = text;
        else if (key == "arguments") terminalArguments = text;
        else if (key == "shell") terminalShell = text;
        else if (key == "colorScheme") terminalColorScheme = text;
        return;
    }

    if (section == "openWith") {
        bool ok = false;
        const QString text = unquote(value, &ok);
        if (ok) {
            openWith.insert(key, text);
        }
        return;
    }

    if (section == "commands") {
        if (commands.isEmpty()) {
            addWarning(lineNumber, "Command value outside [[commands]]");
            return;
        }

        UserCommand &command = commands.last();
        if (key == "requiresSelection" || key == "showOutput") {
            if (value == "true") {
                if (key == "requiresSelection") command.requiresSelection = true;
                else command.showOutput = true;
            } else if (value == "false") {
                if (key == "requiresSelection") command.requiresSelection = false;
                else command.showOutput = false;
            } else {
                addWarning(lineNumber, QString("Invalid boolean value for command %1").arg(key));
            }
            return;
        }

        bool ok = false;
        const QString text = unquote(value, &ok);
        if (!ok) {
            addWarning(lineNumber, QString("Invalid command value for %1").arg(key));
            return;
        }
        if (key == "name") command.name = text;
        else if (key == "command") command.command = text;
        else if (key == "shortcut") command.shortcut = normalizedShortcut(text);
        else if (key == "workingDirectory") command.workingDirectory = text;
        else addWarning(lineNumber, QString("Unknown command key: %1").arg(key));
    }
}

void AppConfig::validateShortcutConflicts()
{
    QHash<QString, QString> ownerByShortcut;
    QHash<QString, QString> effectiveShortcuts = defaultShortcuts();
    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        effectiveShortcuts.insert(it.key(), it.value());
    }

    for (auto it = effectiveShortcuts.constBegin(); it != effectiveShortcuts.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        const QString owner = QString("shortcut.%1").arg(it.key());
        if (ownerByShortcut.contains(it.value())) {
            addWarning(0, QString("Shortcut conflict: %1 is used by %2 and %3")
                .arg(it.value(), ownerByShortcut.value(it.value()), owner));
        } else {
            ownerByShortcut.insert(it.value(), owner);
        }
    }

    for (const UserCommand &command : commands) {
        if (command.shortcut.isEmpty() || command.name.trimmed().isEmpty()) {
            continue;
        }
        const QString owner = QString("command.%1").arg(command.name);
        if (ownerByShortcut.contains(command.shortcut)) {
            addWarning(0, QString("Shortcut conflict: %1 is used by %2 and %3")
                .arg(command.shortcut, ownerByShortcut.value(command.shortcut), owner));
        } else {
            ownerByShortcut.insert(command.shortcut, owner);
        }
    }
}

void AppConfig::addWarning(int lineNumber, const QString &message)
{
    m_warnings << (lineNumber > 0
        ? QString("config.toml:%1: %2").arg(lineNumber).arg(message)
        : message);
}
