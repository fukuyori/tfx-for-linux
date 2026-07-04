#pragma once

#include <QProcess>
#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class QTextEdit;

class CommandOutputPane : public QWidget
{
    Q_OBJECT

public:
    explicit CommandOutputPane(QWidget *parent = nullptr);

public slots:
    void appendOutput(const QString &name,
                      const QString &commandLine,
                      const QString &workingDirectory,
                      int exitCode,
                      QProcess::ExitStatus exitStatus,
                      const QString &stdoutText,
                      const QString &stderrText);

private:
    void showEntry(int row);

    QListWidget *m_historyList;
    QLabel *m_summaryLabel;
    QTextEdit *m_outputText;
};
