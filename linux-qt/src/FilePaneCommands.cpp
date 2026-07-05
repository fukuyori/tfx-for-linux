#include "FilePane.h"
#include "UiText.h"
#include "core/FileOperations.h"
#include "platform/Platform.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <memory>

namespace {

QString shellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace('\'', "'\"'\"'");
    return "'" + quoted + "'";
}

QString scriptsDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("tfx/scripts");
}

QString expandCommandTokens(QString text,
                            const QStringList &paths,
                            const QString &cwd,
                            bool quoteValues)
{
    const QFileInfo first(paths.isEmpty() ? cwd : paths.first());
    const QString dir = paths.isEmpty() ? cwd : first.absolutePath();
    const QString ext = first.suffix();
    const auto value = [quoteValues](const QString &item) {
        return quoteValues ? shellQuote(item) : item;
    };

    QStringList quotedPaths;
    for (const QString &path : paths) {
        quotedPaths << value(path);
    }

    text.replace("{paths}", quotedPaths.join(' '));
    text.replace("{path}", value(paths.isEmpty() ? cwd : paths.first()));
    text.replace("{dir}", value(dir));
    text.replace("{name}", value(first.fileName()));
    text.replace("{stem}", value(first.completeBaseName()));
    text.replace("{ext}", value(ext));
    text.replace("{cwd}", value(cwd));
    text.replace("{scripts}", value(scriptsDirectory()));
    return text;
}

// Walk up from startDir looking for a `.git` entry (directory or file, the
// latter for submodules/worktrees) to decide whether we are inside a work tree.
bool isInsideGitWorkTree(const QString &startDir)
{
    QDir dir(startDir);
    for (;;) {
        if (QFileInfo::exists(dir.filePath(".git"))) {
            return true;
        }
        if (!dir.cdUp()) {
            return false;
        }
    }
}

// Decide whether a command should appear for the current selection, per the
// documented target/selection/extensions/requireGit filters.
bool commandMatches(const UserCommand &command, const QStringList &paths, const QString &cwd)
{
    const QString target = command.target.isEmpty() ? QStringLiteral("any") : command.target;
    const QString selection = command.selection.isEmpty() ? QStringLiteral("any") : command.selection;

    if (command.requireGit && !isInsideGitWorkTree(cwd)) {
        return false;
    }

    // "current" operates on the pane's folder; the selection is irrelevant.
    if (target == "current") {
        return true;
    }

    if (selection == "single" && paths.size() != 1) {
        return false;
    }
    if (selection == "multiple" && paths.size() < 2) {
        return false;
    }

    if (target == "file" || target == "folder") {
        if (paths.isEmpty()) {
            return false;
        }
        for (const QString &path : paths) {
            const QFileInfo info(path);
            if (target == "file" && !info.isFile()) {
                return false;
            }
            if (target == "folder" && !info.isDir()) {
                return false;
            }
        }
    }

    // Extension filter applies to selected files; a bare "*" or empty list
    // means "all". Selected folders carry no extension and are ignored.
    const bool allExtensions = command.extensions.isEmpty()
        || (command.extensions.size() == 1 && command.extensions.first() == "*");
    if (!allExtensions) {
        if (paths.isEmpty()) {
            return false;
        }
        for (const QString &path : paths) {
            const QFileInfo info(path);
            if (info.isDir()) {
                continue;
            }
            if (!command.extensions.contains(info.suffix().toLower())) {
                return false;
            }
        }
    }

    return true;
}

}

void FilePane::setUserCommands(const QList<UserCommand> &commands)
{
    m_userCommands = commands;
}

void FilePane::runUserCommand(int index)
{
    if (index < 0 || index >= m_userCommands.size()) {
        return;
    }

    const UserCommand command = m_userCommands.at(index);
    if (command.name.trimmed().isEmpty() || command.run.trimmed().isEmpty()) {
        return;
    }

    const QStringList paths = selectedLocalPaths();
    if (!commandMatches(command, paths, m_currentPath)) {
        emit statusMessageRequested(UiText::t("This command does not apply to the current selection.",
                                              "このコマンドは現在の選択に適用できません。"));
        return;
    }

    // Working directory: the parent folder of the first selected item, or the
    // pane's current folder for "current"-target commands and empty selections.
    QString requestedWorkingDirectory = m_currentPath;
    if (command.target != "current" && !paths.isEmpty()) {
        requestedWorkingDirectory = QFileInfo(paths.first()).absolutePath();
    }
    const QString effectiveWorkingDirectory = tfx::core::canonicalDirectoryPath(requestedWorkingDirectory);
    if (effectiveWorkingDirectory.isEmpty()) {
        emit statusMessageRequested(UiText::t("Command working directory is not available: %1",
                                              "コマンドの作業ディレクトリを利用できません: %1")
            .arg(requestedWorkingDirectory));
        return;
    }

    const QString expanded = expandCommandTokens(command.run, paths, m_currentPath, true);

    // Resolve the shell: explicit `shell`, then $SHELL, then /bin/sh.
    QString shell = command.shell.trimmed();
    if (shell.isEmpty()) {
        shell = qEnvironmentVariable("SHELL");
    }
    if (shell.isEmpty()) {
        shell = QStringLiteral("/bin/sh");
    }

    // A multi-line body is written to a temporary script file and run through
    // the shell; a single line is passed with -c.
    auto *process = new QProcess(this);
    process->setWorkingDirectory(effectiveWorkingDirectory);
    QStringList arguments;
    std::shared_ptr<QTemporaryFile> scriptFile;
    if (expanded.contains('\n')) {
        scriptFile = std::make_shared<QTemporaryFile>(
            QDir(QDir::tempPath()).filePath("tfx-cmd-XXXXXX"));
        scriptFile->setAutoRemove(true);
        if (scriptFile->open()) {
            scriptFile->write(expanded.toLocal8Bit());
            if (!expanded.endsWith('\n')) {
                scriptFile->write("\n");
            }
            scriptFile->flush();
            arguments << scriptFile->fileName();
        } else {
            scriptFile.reset();
            arguments << QStringLiteral("-c") << expanded;
        }
    } else {
        arguments << QStringLiteral("-c") << expanded;
    }

    const QString displayCommand = expanded.contains('\n') ? command.name : expanded;

    connect(process, &QProcess::errorOccurred, this,
            [this, process, command](QProcess::ProcessError) {
        emit statusMessageRequested(UiText::t("Could not run command %1: %2",
                                              "コマンドを実行できませんでした %1: %2")
            .arg(command.name, process->errorString()));
    });

    if (command.terminal) {
        // Stream stdout/stderr to the terminal pane's Output tab as it arrives.
        emit terminalCommandStarted(QStringLiteral("$ ") + displayCommand);
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            emit terminalCommandOutput(QString::fromLocal8Bit(process->readAllStandardOutput()));
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
            emit terminalCommandOutput(QString::fromLocal8Bit(process->readAllStandardError()));
        });
        connect(process, &QProcess::finished, this,
                [this, process, command, scriptFile](int exitCode, QProcess::ExitStatus exitStatus) {
            emit terminalCommandOutput(QString::fromLocal8Bit(process->readAllStandardOutput()));
            emit terminalCommandOutput(QString::fromLocal8Bit(process->readAllStandardError()));
            const bool failed = exitStatus != QProcess::NormalExit || exitCode != 0;
            emit terminalCommandFinished(failed
                ? UiText::t("[exit %1]", "[終了コード %1]").arg(exitStatus == QProcess::NormalExit
                                                                ? QString::number(exitCode)
                                                                : UiText::t("crashed", "クラッシュ"))
                : UiText::t("[done]", "[完了]"));
            process->deleteLater();
        });
    } else {
        // terminal = false: capture output quietly into the Command Output dock
        // as browsable history. The dock is revealed only on failure; a running
        // pane otherwise stays out of the way (open it from the toolbar).
        constexpr qint64 OutputCapBytes = 1024 * 1024;
        auto stdoutBuffer = std::make_shared<QByteArray>();
        auto stderrBuffer = std::make_shared<QByteArray>();
        auto truncated = std::make_shared<bool>(false);
        const auto appendBounded = [truncated](QByteArray &buffer, const QByteArray &chunk) {
            const qint64 room = OutputCapBytes - buffer.size();
            if (room <= 0) {
                *truncated = true;
                return;
            }
            if (chunk.size() > room) {
                buffer.append(chunk.constData(), room);
                *truncated = true;
            } else {
                buffer.append(chunk);
            }
        };
        connect(process, &QProcess::readyReadStandardOutput, this, [process, stdoutBuffer, appendBounded]() {
            appendBounded(*stdoutBuffer, process->readAllStandardOutput());
        });
        connect(process, &QProcess::readyReadStandardError, this, [process, stderrBuffer, appendBounded]() {
            appendBounded(*stderrBuffer, process->readAllStandardError());
        });
        connect(process, &QProcess::finished, this,
                [this, process, command, expanded, effectiveWorkingDirectory, scriptFile,
                 stdoutBuffer, stderrBuffer, truncated, appendBounded](int exitCode, QProcess::ExitStatus exitStatus) {
            appendBounded(*stdoutBuffer, process->readAllStandardOutput());
            appendBounded(*stderrBuffer, process->readAllStandardError());
            QString stdoutText = QString::fromLocal8Bit(*stdoutBuffer);
            const QString stderrText = QString::fromLocal8Bit(*stderrBuffer);
            if (*truncated) {
                stdoutText += UiText::t("\n[output truncated]", "\n[出力を切り詰めました]");
            }
            const bool failed = exitStatus != QProcess::NormalExit || exitCode != 0;
            emit commandOutputReady(command.name, expanded, effectiveWorkingDirectory,
                                    exitCode, exitStatus, stdoutText, stderrText, failed);
            emit statusMessageRequested(failed
                ? UiText::t("Command failed: %1", "コマンド失敗: %1").arg(command.name)
                : UiText::t("Command finished: %1", "コマンド完了: %1").arg(command.name));
            process->deleteLater();
        });
    }

    process->start(shell, arguments);
}

void FilePane::addUserCommandActions(QMenu *menu, bool hasSelection)
{
    Q_UNUSED(hasSelection);
    if (m_userCommands.isEmpty()) {
        return;
    }

    // User commands are shown flat at the bottom of the menu (not nested under a
    // submenu), separated from the built-in actions by a single divider.
    const QStringList paths = selectedLocalPaths();
    bool addedSeparator = false;
    for (int i = 0; i < m_userCommands.size(); ++i) {
        const UserCommand &command = m_userCommands.at(i);
        if (command.name.trimmed().isEmpty() || command.run.trimmed().isEmpty()) {
            continue;
        }
        if (!commandMatches(command, paths, m_currentPath)) {
            continue;
        }
        if (!addedSeparator) {
            menu->addSeparator();
            addedSeparator = true;
        }
        auto *action = menu->addAction(command.name, this, [this, i]() {
            runUserCommand(i);
        });
        if (!command.shortcut.isEmpty()) {
            action->setShortcut(QKeySequence(command.shortcut));
        }
    }
}
