#include "TerminalPane.h"
#include "platform/Platform.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent),
      m_output(new QPlainTextEdit(this)),
      m_command(new QLineEdit(this)),
      m_process(new QProcess(this)),
      m_workingDirectory(QDir::homePath())
{
    setObjectName("terminalPane");
    setAttribute(Qt::WA_StyledBackground, true);
    m_output->setObjectName("terminalOutput");
    m_command->setObjectName("terminalCommand");
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(3000);
    m_command->setPlaceholderText("command");

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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);
    layout->addLayout(headerLayout);
    layout->addWidget(m_output, 1);
    layout->addLayout(commandLayout);
}

void TerminalPane::setWorkingDirectory(const QString &path)
{
    if (QFileInfo(path).isDir()) {
        m_workingDirectory = path;
    }
}

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
