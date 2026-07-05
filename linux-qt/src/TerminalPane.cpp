#include "TerminalPane.h"
#include "UiText.h"
#include "core/FileOperations.h"
#include "platform/Platform.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef TFX_HAVE_QTERMWIDGET
#include <qtermwidget.h>

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QThread>

#include <csignal>
#include <sys/wait.h>
#endif

QFont TerminalPane::resolveFont(const QString &family, int size)
{
    QFont font;
    if (!family.isEmpty()) {
        font.setFamily(family);
    } else {
        static const QStringList candidates = {
            "DejaVu Sans Mono", "Liberation Mono", "Hack", "Source Code Pro",
            "Ubuntu Mono", "Noto Sans Mono", "monospace"};
        QString chosen;
        for (const QString &candidate : candidates) {
            const QFontInfo info{QFont(candidate)};
            if (info.fixedPitch() && info.family().compare(candidate, Qt::CaseInsensitive) == 0) {
                chosen = candidate;
                break;
            }
        }
        if (chosen.isEmpty()) {
            font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        } else {
            font.setFamily(chosen);
        }
    }
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSize(size > 0 ? size : 11);
    return font;
}

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent),
      m_workingDirectory(QDir::homePath())
{
    setObjectName("terminalPane");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *title = new QLabel("TERMINAL", this);
    title->setObjectName("terminalTitle");

    auto *sigIntButton = new QToolButton(this);
    sigIntButton->setObjectName("terminalActionButton");
    sigIntButton->setText("^C");
    sigIntButton->setToolTip("Send Ctrl+C (interrupt)");
    connect(sigIntButton, &QToolButton::clicked, this, [this]() { sendControlCharacter(0x03); });

    auto *sigQuitButton = new QToolButton(this);
    sigQuitButton->setObjectName("terminalActionButton");
    sigQuitButton->setText("^\\");
    sigQuitButton->setToolTip("Send Ctrl+\\ (quit)");
    connect(sigQuitButton, &QToolButton::clicked, this, [this]() { sendControlCharacter(0x1C); });

    auto *sigStopButton = new QToolButton(this);
    sigStopButton->setObjectName("terminalActionButton");
    sigStopButton->setText("^Z");
    sigStopButton->setToolTip("Send Ctrl+Z (suspend)");
    connect(sigStopButton, &QToolButton::clicked, this, [this]() { sendControlCharacter(0x1A); });

    auto *syncButton = new QToolButton(this);
    syncButton->setObjectName("terminalActionButton");
    syncButton->setText("cwd");
    syncButton->setToolTip("Sync file pane to terminal directory");
    connect(syncButton, &QToolButton::clicked, this, &TerminalPane::requestDirectorySync);

    auto *closeButton = new QToolButton(this);
    closeButton->setObjectName("terminalCloseButton");
    closeButton->setText("x");
    closeButton->setToolTip("Close");
    connect(closeButton, &QToolButton::clicked, this, &TerminalPane::closeRequested);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(title);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(sigIntButton);
    headerLayout->addWidget(sigQuitButton);
    headerLayout->addWidget(sigStopButton);
    headerLayout->addWidget(syncButton);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);
    layout->addLayout(headerLayout);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("terminalTabs");
    m_tabs->setDocumentMode(true);
    layout->addWidget(m_tabs, 1);

    // The "Output" tab receives user-command output (commands with
    // terminal = true). It is shared by both build variants.
    m_outputView = new QPlainTextEdit(this);
    m_outputView->setObjectName("terminalOutput");
    m_outputView->setReadOnly(true);
    m_outputView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_outputView->setMaximumBlockCount(5000);

#ifdef TFX_HAVE_QTERMWIDGET
    m_terminalTab = new QWidget(m_tabs);
    m_terminalTabLayout = new QVBoxLayout(m_terminalTab);
    m_terminalTabLayout->setContentsMargins(0, 0, 0, 0);
    m_terminalTabLayout->setSpacing(0);
    m_font = resolveFont(QString(), 11);
    createTermWidget();
    m_tabs->addTab(m_terminalTab, UiText::t("Terminal", "ターミナル"));
    m_tabs->addTab(m_outputView, UiText::t("Output", "出力"));
#else
    m_output = new QPlainTextEdit(this);
    m_command = new QLineEdit(this);
    m_process = new QProcess(this);
    m_output->setObjectName("terminalOutput");
    m_command->setObjectName("terminalCommand");
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(3000);
    m_command->setPlaceholderText("command");

    auto *runButton = new QPushButton("Run", this);
    connect(runButton, &QPushButton::clicked, this, &TerminalPane::runCommand);
    connect(m_command, &QLineEdit::returnPressed, this, &TerminalPane::runCommand);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_output->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_output->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardError()));
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        m_output->appendPlainText(QString("[exit %1]").arg(exitCode));
    });

    auto *commandLayout = new QHBoxLayout();
    commandLayout->addWidget(m_command, 1);
    commandLayout->addWidget(runButton);

    auto *terminalTab = new QWidget(m_tabs);
    auto *terminalTabLayout = new QVBoxLayout(terminalTab);
    terminalTabLayout->setContentsMargins(0, 0, 0, 0);
    terminalTabLayout->setSpacing(0);
    terminalTabLayout->addWidget(m_output, 1);
    terminalTabLayout->addLayout(commandLayout);
    m_tabs->addTab(terminalTab, UiText::t("Terminal", "ターミナル"));
    m_tabs->addTab(m_outputView, UiText::t("Output", "出力"));
#endif
}

void TerminalPane::showOutputTab()
{
    if (m_tabs && m_outputView) {
        m_tabs->setCurrentWidget(m_outputView);
    }
}

void TerminalPane::focusTerminal()
{
#ifdef TFX_HAVE_QTERMWIDGET
    if (m_tabs && m_terminalTab) {
        m_tabs->setCurrentWidget(m_terminalTab);
    }
    if (m_term) {
        m_term->setFocus();
    }
#else
    if (m_command) {
        m_command->setFocus();
    }
#endif
}

void TerminalPane::beginCommandOutput(const QString &header)
{
    if (!m_outputView) {
        return;
    }
    if (!m_outputView->document()->isEmpty()) {
        m_outputView->appendPlainText(QString());
    }
    if (!header.isEmpty()) {
        m_outputView->appendPlainText(header);
    }
    // Start the command's output on its own line, below the header.
    QTextCursor cursor = m_outputView->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n"));
    m_outputView->setTextCursor(cursor);
    showOutputTab();
    m_outputView->ensureCursorVisible();
}

void TerminalPane::appendCommandOutput(const QString &text)
{
    if (!m_outputView || text.isEmpty()) {
        return;
    }
    // Insert at the end so partial lines (chunks without a trailing newline)
    // continue on the same line as they arrive.
    QTextCursor cursor = m_outputView->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_outputView->setTextCursor(cursor);
    m_outputView->ensureCursorVisible();
}

void TerminalPane::endCommandOutput(const QString &footer)
{
    if (!m_outputView || footer.isEmpty()) {
        return;
    }
    m_outputView->appendPlainText(footer);
    m_outputView->ensureCursorVisible();
}

TerminalPane::~TerminalPane()
{
#ifdef TFX_HAVE_QTERMWIDGET
    shutdownShell();
#endif
}

#ifdef TFX_HAVE_QTERMWIDGET
void TerminalPane::shutdownShell()
{
    if (!m_term || !m_started) {
        return;
    }
    const int pid = m_term->getShellPID();
    if (pid <= 1) {
        return;
    }
    // QTermWidget's own teardown only sends SIGHUP, which a shell that traps
    // it survives as an orphan. Ask with SIGTERM, wait briefly, then
    // force-kill so no shell outlives the application.
    ::kill(pid, SIGTERM);
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 500) {
        if (::waitpid(pid, nullptr, WNOHANG) != 0) {
            return; // exited and reaped, or already gone
        }
        QThread::msleep(10);
    }
    ::kill(pid, SIGKILL);
    ::waitpid(pid, nullptr, WNOHANG);
}
#endif

void TerminalPane::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef TFX_HAVE_QTERMWIDGET
    // A finished shell tears its QTermWidget down (it cannot be restarted), so
    // recreate one before starting a fresh session.
    if (!m_term) {
        createTermWidget();
    }
    if (m_term && !m_started) {
        startTerminal();
    }
#endif
}

void TerminalPane::setColorScheme(const QString &name)
{
#ifdef TFX_HAVE_QTERMWIDGET
    if (!name.isEmpty() && QTermWidget::availableColorSchemes().contains(name)) {
        m_colorScheme = name;
        if (m_term) {
            m_term->setColorScheme(name);
        }
    }
#else
    Q_UNUSED(name);
#endif
}

void TerminalPane::setContentFont(const QFont &font)
{
#ifdef TFX_HAVE_QTERMWIDGET
    m_font = font;
    if (m_term) {
        m_term->setTerminalFont(font);
    }
#else
    if (m_output) {
        m_output->setFont(font);
    }
    if (m_command) {
        m_command->setFont(font);
    }
#endif
}

void TerminalPane::setWorkingDirectory(const QString &path)
{
    // Only remember the directory for the next shell start; a running shell is
    // never moved automatically (it stays where the user left it).
    const QString directory = tfx::core::canonicalDirectoryPath(path);
    if (!directory.isEmpty()) {
        m_workingDirectory = directory;
    }
}

void TerminalPane::openAt(const QString &path)
{
    const QString directory = tfx::core::canonicalDirectoryPath(path);
    if (directory.isEmpty()) {
        return;
    }
    m_workingDirectory = directory;
#ifdef TFX_HAVE_QTERMWIDGET
    // Explicit "open terminal here": cd a running shell to the folder.
    if (m_started && m_term) {
        QString quoted = directory;
        quoted.replace('\'', "'\\''");
        m_term->sendText("cd -- '" + quoted + "'\n");
    }
#endif
}

#ifdef TFX_HAVE_QTERMWIDGET
namespace {

// Breadth-first search of /proc for a tmux client among the shell's
// descendants. The shell's own cwd goes stale inside tmux, so the active
// pane's path has to come from the tmux server instead.
qint64 tmuxClientDescendant(qint64 shellPid)
{
    if (shellPid <= 0) {
        return 0;
    }
    QMultiHash<qint64, qint64> children;
    QHash<qint64, QString> comm;
    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric) {
            continue;
        }
        QFile statFile(QString("/proc/%1/stat").arg(pid));
        if (!statFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray stat = statFile.readAll();
        // "pid (comm) state ppid ..." — comm may contain spaces/parens, so
        // parse around the last ')'.
        const int close = stat.lastIndexOf(')');
        const int open = stat.indexOf('(');
        if (open < 0 || close < open) {
            continue;
        }
        const QList<QByteArray> tail = stat.mid(close + 2).split(' ');
        if (tail.size() < 2) {
            continue;
        }
        children.insert(tail.at(1).toLongLong(), pid);
        comm.insert(pid, QString::fromUtf8(stat.mid(open + 1, close - open - 1)));
    }

    QList<qint64> queue = children.values(shellPid);
    int guard = 0;
    while (!queue.isEmpty() && guard++ < 256) {
        const qint64 pid = queue.takeFirst();
        if (comm.value(pid).startsWith("tmux")) {
            return pid;
        }
        queue.append(children.values(pid));
    }
    return 0;
}

QString runTmux(const QStringList &arguments)
{
    QProcess tmux;
    tmux.setStandardErrorFile(QProcess::nullDevice());
    tmux.start("tmux", arguments);
    if (!tmux.waitForStarted(500) || !tmux.waitForFinished(1000)) {
        tmux.kill();
        tmux.waitForFinished(200);
        return QString();
    }
    if (tmux.exitStatus() != QProcess::NormalExit || tmux.exitCode() != 0) {
        return QString();
    }
    return QString::fromLocal8Bit(tmux.readAllStandardOutput());
}

QString tmuxPaneDirectory(qint64 shellPid)
{
    const qint64 clientPid = tmuxClientDescendant(shellPid);
    if (clientPid <= 0) {
        return QString();
    }
    // Identify the client by its controlling tty, then ask for the active
    // pane of that client's session. (`display-message -c` alone resolves
    // formats against the caller's session, not the target client's.)
    const QString tty = QFile::symLinkTarget(QString("/proc/%1/fd/0").arg(clientPid));
    if (!tty.startsWith("/dev/")) {
        return QString();
    }
    QString sessionId;
    const QString clients = runTmux({"list-clients", "-F", "#{client_tty}\t#{session_id}"});
    const QStringList lines = clients.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int tab = line.indexOf('\t');
        if (tab > 0 && line.left(tab) == tty) {
            sessionId = line.mid(tab + 1);
            break;
        }
    }
    if (sessionId.isEmpty()) {
        return QString();
    }
    const QString path =
        runTmux({"display-message", "-t", sessionId, "-p", "-F", "#{pane_current_path}"}).trimmed();
    return tfx::core::canonicalDirectoryPath(path);
}

}
#endif

QString TerminalPane::currentTerminalDirectory() const
{
#ifdef TFX_HAVE_QTERMWIDGET
    if (m_term) {
        // Inside tmux the shell's cwd is where the client was started; ask
        // the tmux server for the active pane's directory instead.
        const QString tmuxDirectory = tmuxPaneDirectory(m_term->getShellPID());
        if (!tmuxDirectory.isEmpty()) {
            return tmuxDirectory;
        }
    }
    const QString terminalDirectory = m_term ? m_term->workingDirectory() : QString();
    const QString directory = tfx::core::canonicalDirectoryPath(terminalDirectory);
    if (!directory.isEmpty()) {
        return directory;
    }
#endif
    return m_workingDirectory;
}

void TerminalPane::requestDirectorySync()
{
    const QString directory = currentTerminalDirectory();
    if (!directory.isEmpty()) {
        emit directorySyncRequested(directory);
    }
}

void TerminalPane::sendControlCharacter(char code)
{
#ifdef TFX_HAVE_QTERMWIDGET
    if (m_term && m_started) {
        m_term->sendText(QString(QChar(static_cast<ushort>(static_cast<unsigned char>(code)))));
        m_term->setFocus();
    }
#else
    Q_UNUSED(code);
#endif
}

#ifdef TFX_HAVE_QTERMWIDGET
namespace {

QtMessageHandler s_termPreviousHandler = nullptr;

void filterTermConstructionWarning(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    // QTermWidget's constructor applies its own "Monospace" default before
    // our font is set; on CJK systems that resolves to a dual-width font and
    // emits a spurious variable-width warning.
    if (message.contains("variable-width font")) {
        return;
    }
    if (s_termPreviousHandler) {
        s_termPreviousHandler(type, context, message);
    }
}

}

void TerminalPane::createTermWidget()
{
    // Suppress the constructor's font warning only; a genuinely
    // variable-width configured font still warns from setTerminalFont below.
    s_termPreviousHandler = qInstallMessageHandler(filterTermConstructionWarning);
    m_term = new QTermWidget(0, this); // 0: do not start the shell yet
    qInstallMessageHandler(s_termPreviousHandler);
    m_started = false;
    m_term->setTerminalFont(m_font);

    const QStringList schemes = QTermWidget::availableColorSchemes();
    QString scheme = m_colorScheme;
    if (scheme.isEmpty() || !schemes.contains(scheme)) {
        scheme.clear();
        for (const QString &preferred : {QStringLiteral("DarkPastels"), QStringLiteral("Linux"),
                                         QStringLiteral("Solarized Dark")}) {
            if (schemes.contains(preferred)) {
                scheme = preferred;
                break;
            }
        }
    }
    if (!scheme.isEmpty()) {
        m_term->setColorScheme(scheme);
    }

    connect(m_term, &QTermWidget::finished, this, [this]() {
        // The shell exited. QTermWidget cannot restart its session, so discard
        // this widget; showEvent() will build a fresh one in the active
        // directory the next time the pane is shown.
        if (m_term) {
            m_terminalTabLayout->removeWidget(m_term);
            m_term->deleteLater();
            m_term = nullptr;
        }
        m_started = false;
        emit closeRequested();
    });
    m_terminalTabLayout->addWidget(m_term, 1);
}

void TerminalPane::startTerminal()
{
    m_started = true;
    m_term->setWorkingDirectory(m_workingDirectory);
    QStringList environment = QProcess::systemEnvironment();
    environment << QStringLiteral("COLORTERM=truecolor");
    m_term->setEnvironment(environment);
    m_term->setTerminalFont(m_font);
    m_term->startShellProgram();
    // Re-apply the font after the session starts; QTermWidget can otherwise
    // fall back to its default terminal font on shell start.
    m_term->setTerminalFont(m_font);
    m_term->setFocus();
    // A freshly recreated widget may not be laid out yet when the shell starts,
    // and QTermWidget can reset the font during that first layout. Re-apply it
    // once the event loop has realised the terminal.
    QTimer::singleShot(0, this, [this]() {
        if (m_term) {
            m_term->setTerminalFont(m_font);
        }
    });
}
#else
void TerminalPane::runCommand()
{
    if (m_process->state() != QProcess::NotRunning) {
        return;
    }
    const QString command = m_command->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    m_output->appendPlainText(QString("%1 $ %2").arg(m_workingDirectory, command));
    m_command->clear();
    m_process->setWorkingDirectory(m_workingDirectory);
    m_process->start(tfx::platform::terminalShellProgram(), tfx::platform::terminalRunArguments(command));
}
#endif
