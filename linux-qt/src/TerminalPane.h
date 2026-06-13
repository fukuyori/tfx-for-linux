#pragma once

#include <QFont>
#include <QWidget>

#ifndef TFX_HAVE_QTERMWIDGET
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#endif

class QTermWidget;

class TerminalPane : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPane(QWidget *parent = nullptr);
    void setWorkingDirectory(const QString &path);
    void setContentFont(const QFont &font);
    void setColorScheme(const QString &name);

signals:
    void closeRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QString m_workingDirectory;

#ifdef TFX_HAVE_QTERMWIDGET
    void startTerminal();
    QTermWidget *m_term = nullptr;
    QFont m_font;
    bool m_started = false;
#else
    void runCommand();
    QPlainTextEdit *m_output = nullptr;
    QLineEdit *m_command = nullptr;
    QProcess *m_process = nullptr;
#endif
};
