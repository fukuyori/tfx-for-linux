#include "TerminalPane.h"
#include "platform/Platform.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef TFX_HAVE_QTERMWIDGET
#include <qtermwidget.h>

#include <QFontDatabase>
#include <QProcess>
#endif

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent),
      m_workingDirectory(QDir::homePath())
{
    setObjectName("terminalPane");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *title = new QLabel("TERMINAL", this);
    title->setObjectName("terminalTitle");
    auto *closeButton = new QToolButton(this);
    closeButton->setObjectName("terminalCloseButton");
    closeButton->setText("x");
    closeButton->setToolTip("Close");
    connect(closeButton, &QToolButton::clicked, this, &TerminalPane::closeRequested);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);
    layout->addLayout(headerLayout);

#ifdef TFX_HAVE_QTERMWIDGET
    m_term = new QTermWidget(0, this); // 0: do not start the shell yet
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);
    monoFont.setPointSize(11);
    m_font = monoFont;
    m_term->setTerminalFont(m_font);
    const QStringList schemes = QTermWidget::availableColorSchemes();
    for (const QString &preferred : {QStringLiteral("DarkPastels"), QStringLiteral("Linux"),
                                     QStringLiteral("Solarized Dark")}) {
        if (schemes.contains(preferred)) {
            m_term->setColorScheme(preferred);
            break;
        }
    }
    connect(m_term, &QTermWidget::finished, this, &TerminalPane::closeRequested);
    layout->addWidget(m_term, 1);
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

    layout->addWidget(m_output, 1);
    layout->addLayout(commandLayout);
#endif
}

void TerminalPane::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef TFX_HAVE_QTERMWIDGET
    if (m_term && !m_started) {
        startTerminal();
    }
#endif
}

void TerminalPane::setColorScheme(const QString &name)
{
#ifdef TFX_HAVE_QTERMWIDGET
    if (!name.isEmpty() && m_term && QTermWidget::availableColorSchemes().contains(name)) {
        m_term->setColorScheme(name);
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
    if (!QFileInfo(path).isDir()) {
        return;
    }
    m_workingDirectory = path;
#ifdef TFX_HAVE_QTERMWIDGET
    if (m_started && m_term) {
        QString quoted = path;
        quoted.replace('\'', "'\\''");
        m_term->sendText("cd '" + quoted + "'\n");
    }
#endif
}

#ifdef TFX_HAVE_QTERMWIDGET
void TerminalPane::startTerminal()
{
    m_started = true;
    m_term->setWorkingDirectory(m_workingDirectory);
    QStringList environment = QProcess::systemEnvironment();
    environment << QStringLiteral("COLORTERM=truecolor");
    m_term->setEnvironment(environment);
    m_term->startShellProgram();
    // Re-apply the font after the session starts; QTermWidget can otherwise
    // fall back to its default terminal font on shell start.
    m_term->setTerminalFont(m_font);
    m_term->setFocus();
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
