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
    if (command.name.trimmed().isEmpty() || command.command.trimmed().isEmpty()) {
        return;
    }

    const QStringList paths = selectedLocalPaths();
    if (command.requiresSelection && paths.isEmpty()) {
        emit statusMessageRequested(UiText::t("Select an item before running this command.",
                                              "このコマンドを実行するには項目を選択してください。"));
        return;
    }

    const QString workingDirectory = expandCommandTokens(command.workingDirectory, paths, m_currentPath, false);
    const QString expanded = expandCommandTokens(command.command, paths, m_currentPath, true);
    auto *process = new QProcess(this);
    const QString requestedWorkingDirectory = workingDirectory.isEmpty() ? m_currentPath : workingDirectory;
    const QString effectiveWorkingDirectory = tfx::core::canonicalDirectoryPath(requestedWorkingDirectory);
    if (effectiveWorkingDirectory.isEmpty()) {
        emit statusMessageRequested(UiText::t("Command working directory is not available: %1",
                                              "コマンドの作業ディレクトリを利用できません: %1")
            .arg(requestedWorkingDirectory));
        process->deleteLater();
        return;
    }
    process->setWorkingDirectory(effectiveWorkingDirectory);

    // Read output incrementally with a hard cap so a command that floods
    // stdout/stderr cannot grow the process buffers without bound.
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
            [this, process, command, expanded, effectiveWorkingDirectory,
             stdoutBuffer, stderrBuffer, truncated, appendBounded](int exitCode, QProcess::ExitStatus exitStatus) {
        appendBounded(*stdoutBuffer, process->readAllStandardOutput());
        appendBounded(*stderrBuffer, process->readAllStandardError());
        QString stdoutText = QString::fromLocal8Bit(*stdoutBuffer);
        const QString stderrText = QString::fromLocal8Bit(*stderrBuffer);
        if (*truncated) {
            stdoutText += UiText::t("\n[output truncated]", "\n[出力を切り詰めました]");
        }
        const bool failed = exitStatus != QProcess::NormalExit || exitCode != 0;
        emit commandOutputReady(command.name,
                                expanded,
                                effectiveWorkingDirectory,
                                exitCode,
                                exitStatus,
                                stdoutText,
                                stderrText,
                                command.showOutput || failed);
        if (!command.showOutput && !failed) {
            emit statusMessageRequested(UiText::t("Command finished: %1", "コマンド完了: %1").arg(command.name));
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, command](QProcess::ProcessError) {
        emit statusMessageRequested(UiText::t("Could not run command %1: %2",
                                              "コマンドを実行できませんでした %1: %2")
            .arg(command.name, process->errorString()));
    });
    process->start(tfx::platform::terminalShellProgram(), tfx::platform::terminalRunArguments(expanded));
}

void FilePane::addUserCommandActions(QMenu *menu, bool hasSelection)
{
    if (m_userCommands.isEmpty()) {
        return;
    }

    QMenu *commandsMenu = nullptr;
    for (int i = 0; i < m_userCommands.size(); ++i) {
        const UserCommand &command = m_userCommands.at(i);
        if (command.name.trimmed().isEmpty() || command.command.trimmed().isEmpty()) {
            continue;
        }
        if (!commandsMenu) {
            menu->addSeparator();
            commandsMenu = menu->addMenu(UiText::t("Commands", "コマンド"));
        }
        auto *action = commandsMenu->addAction(command.name, this, [this, i]() {
            runUserCommand(i);
        });
        action->setEnabled(hasSelection || !command.requiresSelection);
        if (!command.shortcut.isEmpty()) {
            action->setShortcut(QKeySequence(command.shortcut));
        }
    }
}
