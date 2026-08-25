#pragma once

#include <QFont>
#include <QPlainTextEdit>
#include <QWidget>

#ifndef TFX_HAVE_QTERMWIDGET
#include <QLineEdit>
#include <QProcess>
#endif

class QTermWidget;
class QTabWidget;
class QVBoxLayout;

class TerminalPane : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPane(QWidget *parent = nullptr);
    ~TerminalPane() override;
    void setWorkingDirectory(const QString &path);
    void openAt(const QString &path);
    void setContentFont(const QFont &font);
    void setColorScheme(const QString &name);
    // Background opacity of the terminal itself. QTermWidget paints its colour
    // scheme opaquely, so without this the terminal stays solid inside an
    // otherwise translucent window.
    void setBackgroundOpacity(double level);

    // Stream user-command output into the "Output" tab. beginCommandOutput
    // switches to the tab and prints a header; appendCommandOutput adds a chunk
    // as it arrives; endCommandOutput prints a trailing footer.
    void beginCommandOutput(const QString &header);
    void appendCommandOutput(const QString &text);
    void endCommandOutput(const QString &footer);

    // Switch to the interactive Terminal tab and give it keyboard focus.
    void focusTerminal();

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
    void sendControlCharacter(char code);
    void showOutputTab();
    QString m_workingDirectory;

    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_outputView = nullptr;

#ifdef TFX_HAVE_QTERMWIDGET
    void createTermWidget();
    void startTerminal();
    void shutdownShell();
    QWidget *m_terminalTab = nullptr;
    QVBoxLayout *m_terminalTabLayout = nullptr;
    QTermWidget *m_term = nullptr;
    double m_backgroundOpacity = 1.0;
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
