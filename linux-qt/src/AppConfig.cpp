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
        "\n"
        "[shortcuts]\n"
        "reload = \"f5\"\n"
        "openTerminal = \"ctrl+shift+t\"\n"
        "togglePreview = \"ctrl+shift+p\"\n"
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
        "# rightFolder = \"~/Downloads\"\n"
        "# rightFolders = [\"~/Downloads\", \"~/Documents\"]\n"
        "\n"
        "# [colors]\n"
        "# fileListBackground = \"#151A1E\"\n"
        "# fileForeground = \"#D9E1E8\"\n"
        "# directoryForeground = \"#E5EDF3\"\n"
        "# paneBorderKeyboardTarget = \"#36E67A\"\n"
        "\n"
        "# [terminal]\n"
        "# app = \"x-terminal-emulator\"\n"
        "# arguments = \"--working-directory={path}\"\n"
        "\n"
        "# [openWith]\n"
        "# md = \"code\"\n");
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
        if (key == "size") {
            bool ok = false;
            const int size = value.toInt(&ok);
            if (ok && size >= 8 && size <= 40) {
                font.size = size;
            } else {
                addWarning(lineNumber, "Invalid font size");
            }
            return;
        }
        if (key == "ui" || key == "mono") {
            bool ok = false;
            const QString text = unquote(value, &ok);
            if (ok) {
                key == "ui" ? font.ui = text : font.mono = text;
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

    if (section == "startup") {
        bool ok = false;
        if (key == "layout" || key == "preview" || key == "rightFolder") {
            const QString text = unquote(value, &ok);
            if (!ok) {
                addWarning(lineNumber, "Invalid startup value");
                return;
            }
            if (key == "layout" && (text == "single" || text == "split" || text == "restore")) startup.layout = text;
            else if (key == "preview" && (text == "show" || text == "hide" || text == "restore")) startup.preview = text;
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
        return;
    }

    if (section == "openWith") {
        bool ok = false;
        const QString text = unquote(value, &ok);
        if (ok) {
            openWith.insert(key, text);
        }
    }
}

void AppConfig::addWarning(int lineNumber, const QString &message)
{
    m_warnings << (lineNumber > 0
        ? QString("config.toml:%1: %2").arg(lineNumber).arg(message)
        : message);
}
