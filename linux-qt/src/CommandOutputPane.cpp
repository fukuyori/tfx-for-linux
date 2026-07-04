#include "CommandOutputPane.h"
#include "UiText.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

CommandOutputPane::CommandOutputPane(QWidget *parent)
    : QWidget(parent),
      m_historyList(new QListWidget(this)),
      m_summaryLabel(new QLabel(this)),
      m_outputText(new QTextEdit(this))
{
    setObjectName("commandOutputPane");
    m_historyList->setObjectName("commandHistoryList");
    m_historyList->setMinimumHeight(92);
    m_historyList->setMaximumHeight(150);

    m_summaryLabel->setObjectName("sectionLabel");
    m_summaryLabel->setText(UiText::t("No command output yet.", "コマンド出力はまだありません。"));

    m_outputText->setObjectName("commandOutputText");
    m_outputText->setReadOnly(true);
    m_outputText->setLineWrapMode(QTextEdit::NoWrap);

    auto *clearButton = new QPushButton(UiText::t("Clear", "クリア"), this);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_historyList->clear();
        m_summaryLabel->setText(UiText::t("No command output yet.", "コマンド出力はまだありません。"));
        m_outputText->clear();
    });

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(m_summaryLabel, 1);
    headerLayout->addWidget(clearButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_historyList);
    layout->addLayout(headerLayout);
    layout->addWidget(m_outputText, 1);

    connect(m_historyList, &QListWidget::currentRowChanged, this, &CommandOutputPane::showEntry);
}

void CommandOutputPane::appendOutput(const QString &name,
                                     const QString &commandLine,
                                     const QString &workingDirectory,
                                     int exitCode,
                                     QProcess::ExitStatus exitStatus,
                                     const QString &stdoutText,
                                     const QString &stderrText)
{
    const QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
    const QString status = exitStatus == QProcess::NormalExit
        ? QString::number(exitCode)
        : UiText::t("crashed", "クラッシュ");
    auto *item = new QListWidgetItem(QString("[%1] %2 (%3)").arg(time, name, status), m_historyList);

    QString details;
    details += UiText::t("Command: %1\n", "コマンド: %1\n").arg(name);
    details += UiText::t("Working directory: %1\n", "作業ディレクトリ: %1\n").arg(workingDirectory);
    details += UiText::t("Exit: %1\n", "終了: %1\n").arg(status);
    details += "\n$ " + commandLine + "\n\n";
    if (!stdoutText.trimmed().isEmpty()) {
        details += "stdout\n------\n" + stdoutText.trimmed() + "\n\n";
    }
    if (!stderrText.trimmed().isEmpty()) {
        details += "stderr\n------\n" + stderrText.trimmed() + "\n";
    }
    if (stdoutText.trimmed().isEmpty() && stderrText.trimmed().isEmpty()) {
        details += UiText::t("No output.", "出力はありません。");
    }

    item->setData(Qt::UserRole, details);
    item->setData(Qt::UserRole + 1, ok);
    m_historyList->setCurrentItem(item);
}

void CommandOutputPane::showEntry(int row)
{
    if (row < 0) {
        return;
    }
    auto *item = m_historyList->item(row);
    if (!item) {
        return;
    }
    const bool ok = item->data(Qt::UserRole + 1).toBool();
    m_summaryLabel->setText(ok ? UiText::t("Command completed.", "コマンドが完了しました。")
                               : UiText::t("Command failed.", "コマンドが失敗しました。"));
    m_outputText->setPlainText(item->data(Qt::UserRole).toString());
}
