#include "controllers/GitStatusController.h"

#include "UiText.h"
#include "core/GitService.h"

#include <QDir>
#include <QFileInfo>
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
    m_directory = directory;

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
    if (gitProgram.isEmpty() || directory.isEmpty()) {
        emit branchChanged(QString());
        return;
    }

    auto *branchProcess = new QProcess(this);
    branchProcess->setProgram(gitProgram);
    branchProcess->setArguments({"-C", directory, "rev-parse", "--abbrev-ref", "HEAD"});
    connect(branchProcess, &QProcess::finished, this,
            [this, branchProcess, directory](int code, QProcess::ExitStatus status) {
        QString branch;
        if (status == QProcess::NormalExit && code == 0) {
            branch = QString::fromLocal8Bit(branchProcess->readAllStandardOutput()).trimmed();
            if (branch == "HEAD") {
                branch = UiText::t("detached", "detached");
            }
        }
        branchProcess->deleteLater();
        if (directory == m_directory) {
            emit branchChanged(branch);
        }
    });
    branchProcess->start();

    m_statusProcess = new QProcess(this);
    m_statusProcess->setProgram(gitProgram);
    m_statusProcess->setArguments({"-C", directory, "status", "--porcelain=v1", "--untracked-files=all"});
    connect(m_statusProcess, &QProcess::finished, this,
            [this, directory](int exitCode, QProcess::ExitStatus exitStatus) {
        QProcess *process = m_statusProcess;
        m_statusProcess = nullptr;
        if (!process) {
            return;
        }
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0 && directory == m_directory) {
            applyStatusOutput(directory, output);
        }
    });
    m_statusProcess->start();
}

void GitStatusController::applyStatusOutput(const QString &directory, const QString &output)
{
    QHash<QString, QString> statuses;
    const QDir dir(directory);
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
        const QString absolutePath = QFileInfo(dir.filePath(relativePath)).absoluteFilePath();
        statuses.insert(absolutePath, label);

        const QString topLevelName = relativePath.section('/', 0, 0);
        if (!topLevelName.isEmpty()) {
            const QString topLevelPath = QFileInfo(dir.filePath(topLevelName)).absoluteFilePath();
            if (!statuses.contains(topLevelPath)) {
                statuses.insert(topLevelPath, label);
            }
        }
    }
    emit statusesReady(statuses);
}
