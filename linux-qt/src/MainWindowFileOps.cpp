#include "MainWindow.h"
#include "UiText.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QTimer>

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_fileOperationWorker) {
        const int pendingCount = queuedFileOperationCount();
        const QString prompt = pendingCount > 0
            ? UiText::t("A file operation is still running and %1 item(s) are queued. Cancel them and quit?",
                        "ファイル操作を実行中で、%1 件が待機中です。キャンセルして終了しますか？").arg(pendingCount)
            : UiText::t("A file operation is still running. Cancel it and quit?",
                        "ファイル操作を実行中です。キャンセルして終了しますか？");
        const auto result = QMessageBox::question(
            this,
            UiText::t("File Operation Running", "ファイル操作を実行中"),
            prompt);
        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_queuedFileOperations.clear();
        m_closeAfterFileOperationCancel = true;
        cancelFileOperation();
        event->ignore();
        return;
    }
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::startFileOperation(const QVector<FileOperationRequest> &requests)
{
    if (requests.isEmpty()) {
        return;
    }
    if (m_fileOperationWorker) {
        m_queuedFileOperations += requests;
        updateFileOperationSummary();
        statusBar()->showMessage(
            UiText::t("File operation queued (%1 pending item(s)).", "ファイル操作をキューに追加しました（%1 件待機中）。")
                .arg(queuedFileOperationCount()),
            3000);
        return;
    }

    m_fileOperationThread = new QThread(this);
    m_fileOperationWorker = new FileOperationWorker(requests);
    m_fileOperationWorker->moveToThread(m_fileOperationThread);
    m_lastFileOperationCompleted = 0;
    m_lastFileOperationTotal = 0;

    m_fileOperationProgress->setRange(0, 0);
    m_fileOperationProgress->setValue(0);
    m_fileOperationSummary->show();
    m_fileOperationProgress->show();
    m_fileOperationCancel->setEnabled(true);
    m_fileOperationCancel->show();
    updateFileOperationSummary(0, 0);
    statusBar()->showMessage(UiText::t("File operation running...", "ファイル操作を実行中..."));

    connect(m_fileOperationThread, &QThread::started, m_fileOperationWorker, &FileOperationWorker::run);
    connect(m_fileOperationThread, &QThread::finished, m_fileOperationThread, &QObject::deleteLater);
    connect(m_fileOperationWorker, &FileOperationWorker::prepared, this,
            [this](int total) {
        m_fileOperationProgress->setRange(0, qMax(1, total));
        m_fileOperationProgress->setValue(0);
        updateFileOperationSummary(0, total);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::progress, this,
            [this](int completed, int total, const QString &path) {
        m_fileOperationProgress->setRange(0, qMax(1, total));
        m_fileOperationProgress->setValue(qMin(completed, total));
        updateFileOperationSummary(completed, total);
        statusBar()->showMessage(UiText::t("Transferring: %1", "転送中: %1").arg(QFileInfo(path).fileName()));
    });
    connect(m_fileOperationWorker, &FileOperationWorker::finished, this,
            [this](const QStringList &directories) {
        completeFileOperation(directories, UiText::t("File operation completed.", "ファイル操作が完了しました。"), true);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::failed, this,
            [this](const QString &message, const QStringList &directories) {
        completeFileOperation(directories, message, false);
    });
    connect(m_fileOperationWorker, &FileOperationWorker::canceled, this,
            [this](const QStringList &directories) {
        completeFileOperation(directories, UiText::t("File operation canceled.", "ファイル操作をキャンセルしました。"), false);
    });

    m_fileOperationThread->start();
}

void MainWindow::cancelFileOperation()
{
    if (!m_fileOperationWorker) {
        return;
    }
    m_queuedFileOperations.clear();
    updateFileOperationSummary();
    m_fileOperationCancel->setEnabled(false);
    statusBar()->showMessage(UiText::t("Canceling file operation and clearing the queue...",
                                      "ファイル操作をキャンセルし、キューをクリアしています..."));
    m_fileOperationWorker->cancel();
}

void MainWindow::completeFileOperation(const QStringList &directories, const QString &message, bool continueQueued)
{
    reloadChangedDirectories(directories);

    const int pendingCount = queuedFileOperationCount();
    QString finalMessage = message;
    if (!continueQueued && pendingCount > 0) {
        m_queuedFileOperations.clear();
        finalMessage += UiText::t(" Queued item(s) cleared: %1.", " 待機中の項目をクリアしました: %1 件。").arg(pendingCount);
    }

    FileOperationWorker *worker = m_fileOperationWorker;
    QThread *thread = m_fileOperationThread;
    const bool closeAfterCancel = m_closeAfterFileOperationCancel;
    m_fileOperationWorker = nullptr;
    m_fileOperationThread = nullptr;
    m_closeAfterFileOperationCancel = false;

    if (thread && closeAfterCancel) {
        connect(thread, &QThread::finished, this, [this]() {
            QTimer::singleShot(0, this, &QWidget::close);
        });
    }
    if (worker) {
        worker->deleteLater();
    }
    if (thread) {
        thread->quit();
    }

    m_fileOperationProgress->hide();
    m_fileOperationSummary->hide();
    m_fileOperationCancel->hide();
    m_lastFileOperationCompleted = 0;
    m_lastFileOperationTotal = 0;
    statusBar()->showMessage(finalMessage, 5000);

    if (closeAfterCancel) {
        if (!thread) {
            QTimer::singleShot(0, this, &QWidget::close);
        }
        return;
    }

    if (continueQueued && !m_queuedFileOperations.isEmpty()) {
        const QVector<FileOperationRequest> queued = m_queuedFileOperations;
        m_queuedFileOperations.clear();
        QTimer::singleShot(0, this, [this, queued]() {
            startFileOperation(queued);
        });
    }
}

void MainWindow::reloadChangedDirectories(const QStringList &directories)
{
    for (FilePane *candidate : {m_leftPane, m_rightPane}) {
        if (directories.contains(candidate->currentPath())) {
            candidate->reload();
        }
    }
}

int MainWindow::queuedFileOperationCount() const
{
    return m_queuedFileOperations.size();
}

void MainWindow::updateFileOperationSummary(int completed, int total)
{
    if (!m_fileOperationSummary || !m_fileOperationProgress) {
        return;
    }

    if (total >= 0) {
        m_lastFileOperationCompleted = qMax(0, completed);
        m_lastFileOperationTotal = qMax(0, total);
    }

    QString text = m_lastFileOperationTotal > 0
        ? UiText::t("Files %1/%2", "ファイル %1/%2").arg(m_lastFileOperationCompleted).arg(m_lastFileOperationTotal)
        : UiText::t("Preparing", "準備中");

    const int queuedCount = queuedFileOperationCount();
    if (queuedCount > 0) {
        text += UiText::t(" | queued %1", " | 待機 %1").arg(queuedCount);
    }
    m_fileOperationSummary->setText(text);
    m_fileOperationProgress->setFormat(m_lastFileOperationTotal > 0
        ? QString("%1/%2").arg(m_lastFileOperationCompleted).arg(m_lastFileOperationTotal)
        : QString());
}
