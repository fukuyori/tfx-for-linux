#pragma once

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QWidget>

class TerminalPane : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPane(QWidget *parent = nullptr);
    void setWorkingDirectory(const QString &path);

signals:
    void closeRequested();

private slots:
    void runCommand();

private:
    QPlainTextEdit *m_output;
    QLineEdit *m_command;
    QProcess *m_process;
    QString m_workingDirectory;
};
