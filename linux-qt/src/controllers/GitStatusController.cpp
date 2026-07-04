#include "controllers/GitStatusController.h"

#include "UiText.h"
#include "core/FileOperations.h"
#include "core/GitService.h"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

using namespace tfx::core;

GitStatusController::GitStatusController(QObject *parent)
    : QObject(parent)
{
}

void GitStatusController::refresh(const QString &directory)
{
    const QString gitDirectory = canonicalDirectoryPath(directory);
    m_directory = gitDirectory;

    if (m_statusProcess) {
        disconnect(m_statusProcess, nullptr, this, nullptr);
        m_statusProcess->kill();
        m_statusProcess->waitForFinished(100);
        m_statusProcess->deleteLater();
        m_statusProcess = nullptr;
    }

    // Clear badges immediately; they are repopulated when the run completes.
    emit statusesReady({});

    const QString gitProgram = QStandardPaths::findExecutable("git");
    if (gitProgram.isEmpty() || gitDirectory.isEmpty()) {
        emit branchChanged(QString());
        return;
    }

    auto *branchProcess = new QProcess(this);
    branchProcess->setProgram(gitProgram);
    branchProcess->setArguments({"-C", gitDirectory, "rev-parse", "--abbrev-ref", "HEAD"});
    connect(branchProcess, &QProcess::finished, this,
            [this, branchProcess, gitDirectory](int code, QProcess::ExitStatus status) {
        QString branch;
        if (status == QProcess::NormalExit && code == 0) {
            branch = QString::fromLocal8Bit(branchProcess->readAllStandardOutput()).trimmed();
            if (branch == "HEAD") {
                branch = UiText::t("detached", "detached");
            }
        }
        branchProcess->deleteLater();
        if (gitDirectory == m_directory) {
            emit branchChanged(branch);
        }
    });
    branchProcess->start();

    m_statusProcess = new QProcess(this);
    m_statusProcess->setProgram(gitProgram);
    m_statusProcess->setArguments({"-C", gitDirectory, "status", "--porcelain=v1", "--untracked-files=all"});
    connect(m_statusProcess, &QProcess::finished, this,
            [this, gitDirectory](int exitCode, QProcess::ExitStatus exitStatus) {
        QProcess *process = m_statusProcess;
        m_statusProcess = nullptr;
        if (!process) {
            return;
        }
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0 && gitDirectory == m_directory) {
            applyStatusOutput(gitDirectory, output);
        }
    });
    m_statusProcess->start();
}

void GitStatusController::applyStatusOutput(const QString &directory, const QString &output)
{
    QHash<QString, QString> statuses;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.size() < 4) {
            continue;
        }
        const QString status = line.left(2);
        const QString relativePath = porcelainPath(line.mid(3));
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
