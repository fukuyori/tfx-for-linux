#include "controllers/GitStatusController.h"

#include "UiText.h"
#include "core/FileOperations.h"
#include "core/GitService.h"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

using namespace tfx::core;

namespace {

constexpr int RefreshThrottleMs = 1000;
constexpr int WatchdogTerminateMs = 30000;
constexpr int WatchdogKillMs = 5000;

// Stop a git process that outlives its useful lifetime: terminate after
// 30 seconds, escalate to kill 5 seconds later. The timers die with the
// process object, so a normal finish cancels the escalation.
void attachWatchdog(QProcess *process)
{
    auto *watchdog = new QTimer(process);
    watchdog->setSingleShot(true);
    watchdog->setInterval(WatchdogTerminateMs);
    QObject::connect(watchdog, &QTimer::timeout, process, [process]() {
        process->terminate();
        QTimer::singleShot(WatchdogKillMs, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    });
    QObject::connect(process, &QProcess::finished, watchdog, &QTimer::stop);
    watchdog->start();
}

}

GitStatusController::GitStatusController(QObject *parent)
    : QObject(parent)
{
    m_throttle = new QTimer(this);
    m_throttle->setSingleShot(true);
    connect(m_throttle, &QTimer::timeout, this, [this]() {
        if (!m_pendingDirectory.isEmpty()) {
            startRefresh(m_pendingDirectory);
        }
    });
}

void GitStatusController::refresh(const QString &directory)
{
    const QString gitDirectory = canonicalDirectoryPath(directory);

    // Refreshes of the directory already shown (watcher storms, periodic
    // polling) are throttled; navigating somewhere else runs immediately.
    if (gitDirectory == m_directory && m_lastStart.isValid()
        && m_lastStart.elapsed() < RefreshThrottleMs) {
        m_pendingDirectory = gitDirectory;
        if (!m_throttle->isActive()) {
            m_throttle->start(qMax(qint64(0), RefreshThrottleMs - m_lastStart.elapsed()));
        }
        return;
    }

    startRefresh(gitDirectory);
}

void GitStatusController::startRefresh(const QString &gitDirectory)
{
    m_pendingDirectory.clear();
    m_throttle->stop();
    m_lastStart.start();
    m_directory = gitDirectory;

    stopProcess(m_statusProcess);
    stopProcess(m_prefixProcess);
    stopProcess(m_branchProcess);

    // Clear badges immediately; they are repopulated when the run completes.
    emit statusesReady({});

    const QString gitProgram = QStandardPaths::findExecutable("git");
    if (gitProgram.isEmpty() || gitDirectory.isEmpty()) {
        emit branchChanged(QString());
        return;
    }

    m_branchProcess = new QProcess(this);
    m_branchProcess->setProgram(gitProgram);
    m_branchProcess->setArguments({"-C", gitDirectory, "rev-parse", "--abbrev-ref", "HEAD"});
    m_branchProcess->setStandardErrorFile(QProcess::nullDevice());
    connect(m_branchProcess, &QProcess::finished, this,
            [this, gitDirectory](int code, QProcess::ExitStatus status) {
        QProcess *process = m_branchProcess;
        m_branchProcess = nullptr;
        if (!process) {
            return;
        }
        QString branch;
        if (status == QProcess::NormalExit && code == 0) {
            branch = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
            if (branch == "HEAD") {
                branch = UiText::t("detached", "detached");
            }
        }
        process->deleteLater();
        if (gitDirectory == m_directory) {
            emit branchChanged(branch);
        }
    });
    attachWatchdog(m_branchProcess);
    m_branchProcess->start();

    // Porcelain paths are relative to the repository root, so resolve the
    // queried directory's prefix inside the repository first, then scope the
    // status run to that directory with a pathspec.
    m_prefixProcess = new QProcess(this);
    m_prefixProcess->setProgram(gitProgram);
    m_prefixProcess->setArguments({"-C", gitDirectory, "rev-parse", "--show-prefix"});
    m_prefixProcess->setStandardErrorFile(QProcess::nullDevice());
    connect(m_prefixProcess, &QProcess::finished, this,
            [this, gitProgram, gitDirectory](int code, QProcess::ExitStatus status) {
        QProcess *process = m_prefixProcess;
        m_prefixProcess = nullptr;
        if (!process) {
            return;
        }
        const QString prefix = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || gitDirectory != m_directory) {
            return; // not a repository, or a newer refresh superseded this one
        }
        startStatus(gitProgram, gitDirectory, prefix);
    });
    attachWatchdog(m_prefixProcess);
    m_prefixProcess->start();
}

void GitStatusController::startStatus(const QString &gitProgram, const QString &gitDirectory, const QString &prefix)
{
    m_statusProcess = new QProcess(this);
    m_statusProcess->setProgram(gitProgram);
    m_statusProcess->setArguments(
        {"-C", gitDirectory, "status", "--porcelain=v1", "--untracked-files=all", "--", "."});
    m_statusProcess->setStandardErrorFile(QProcess::nullDevice());
    connect(m_statusProcess, &QProcess::finished, this,
            [this, gitDirectory, prefix](int exitCode, QProcess::ExitStatus exitStatus) {
        QProcess *process = m_statusProcess;
        m_statusProcess = nullptr;
        if (!process) {
            return;
        }
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0 && gitDirectory == m_directory) {
            applyStatusOutput(gitDirectory, prefix, output);
        }
    });
    attachWatchdog(m_statusProcess);
    m_statusProcess->start();
}

void GitStatusController::stopProcess(QProcess *&process)
{
    if (!process) {
        return;
    }
    disconnect(process, nullptr, this, nullptr);
    process->kill();
    process->waitForFinished(100);
    process->deleteLater();
    process = nullptr;
}

void GitStatusController::applyStatusOutput(const QString &directory, const QString &prefix, const QString &output)
{
    QHash<QString, QString> statuses;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.size() < 4) {
            continue;
        }
        const QString status = line.left(2);
        const QString rootRelativePath = porcelainPath(line.mid(3));
        const QString relativePath = porcelainPathInDirectory(rootRelativePath, prefix);
        if (relativePath.isEmpty()) {
            continue;
        }

        const QString label = porcelainStatusLabel(status);
        const QString absolutePath = porcelainAbsolutePath(directory, relativePath);
        if (absolutePath.isEmpty()) {
            continue;
        }
        statuses.insert(absolutePath, label);

        const QString topLevelName = relativePath.section('/', 0, 0);
        if (!topLevelName.isEmpty()) {
            const QString topLevelPath = porcelainAbsolutePath(directory, topLevelName);
            if (!topLevelPath.isEmpty() && !statuses.contains(topLevelPath)) {
                statuses.insert(topLevelPath, label);
            }
        }
    }
    emit statusesReady(statuses);
}
