#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>

struct FileOperationRequest
{
    QString source;
    QString destination;
    bool move = false;
};

class FileOperationWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileOperationWorker(const QVector<FileOperationRequest> &requests, QObject *parent = nullptr);

public slots:
    void run();
    void cancel();

signals:
    void prepared(int total);
    void progress(int completed, int total, const QString &path);
    void finished(const QStringList &changedDirectories);
    void failed(const QString &message, const QStringList &changedDirectories);
    void canceled(const QStringList &changedDirectories);

private:
    int countEntries(const QString &path) const;
    bool copyPath(const QString &source, const QString &destination, QString *errorText);
    bool copyFile(const QString &source, const QString &destination, QString *errorText);
    bool copyDirectory(const QString &source, const QString &destination, QString *errorText);
    bool removePath(const QString &path);
    void cleanupCreatedRoots();
    void emitStep(const QString &path);
    QStringList changedDirectories() const;
    bool isCanceled() const;

    QVector<FileOperationRequest> m_requests;
    QStringList m_createdRoots;
    QStringList m_changedDirectories;
    std::atomic_bool m_cancelRequested = false;
    int m_completed = 0;
    int m_total = 0;
};
