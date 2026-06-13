#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;

// Runs `git` asynchronously for a directory and reports the current branch and
// per-path status badges. Stale results (after a newer refresh) are dropped.
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
    void applyStatusOutput(const QString &directory, const QString &output);

    QProcess *m_statusProcess = nullptr;
    QString m_directory;
};
