#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>

class QProcess;
class QTimer;

// Runs `git` asynchronously for a directory and reports the current branch and
// per-path status badges. Stale results (after a newer refresh) are dropped.
// Every git invocation carries a watchdog (terminate after 30 s, kill after 5
// more), stderr is discarded, and the status query is scoped to the directory
// with a pathspec. Repeated refreshes of the same directory are throttled to
// at most one per second; navigating to a different directory is immediate.
class GitStatusController : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusController(QObject *parent = nullptr);

    // Query branch and status for `directory`; cancels any in-flight status run.
    void refresh(const QString &directory);

signals:
    void branchChanged(const QString &branch);                 // empty if not a repo
    void statusesReady(const QHash<QString, QString> &statuses); // absolute path -> badge

private:
    void startRefresh(const QString &gitDirectory);
    void startStatus(const QString &gitProgram, const QString &gitDirectory, const QString &prefix);
    void stopProcess(QProcess *&process);
    void applyStatusOutput(const QString &directory, const QString &prefix, const QString &output);

    QProcess *m_branchProcess = nullptr;
    QProcess *m_prefixProcess = nullptr;
    QProcess *m_statusProcess = nullptr;
    QString m_directory;
    QString m_pendingDirectory;
    QTimer *m_throttle = nullptr;
    QElapsedTimer m_lastStart;
};
