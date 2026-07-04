#pragma once

#include <QFont>
#include <QWidget>

#ifndef TFX_HAVE_QTERMWIDGET
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#endif

class QTermWidget;
class QVBoxLayout;

class TerminalPane : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPane(QWidget *parent = nullptr);
    void setWorkingDirectory(const QString &path);
    void openAt(const QString &path);
    void setContentFont(const QFont &font);
    void setColorScheme(const QString &name);

    // Resolve a terminal font. With an explicit family, that family is used;
    // otherwise the first candidate that QFontInfo reports as fixed-pitch is
    // chosen (the generic "Monospace" alias is not always reported as fixed).
    static QFont resolveFont(const QString &family, int size);

signals:
    void closeRequested();
    void directorySyncRequested(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;

private:
    QString currentTerminalDirectory() const;
    void requestDirectorySync();
    QString m_workingDirectory;

#ifdef TFX_HAVE_QTERMWIDGET
    void createTermWidget();
    void startTerminal();
    QVBoxLayout *m_layout = nullptr;
    QTermWidget *m_term = nullptr;
    QFont m_font;
    QString m_colorScheme;
    bool m_started = false;
#else
    void runCommand();
    QPlainTextEdit *m_output = nullptr;
    QLineEdit *m_command = nullptr;
    QProcess *m_process = nullptr;
#endif
};
