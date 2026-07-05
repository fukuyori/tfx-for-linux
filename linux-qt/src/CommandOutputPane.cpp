#include "CommandOutputPane.h"
#include "UiText.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
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
    m_historyList->setMinimumHeight(48);

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

    // The output section (summary + text) forms the lower half so the whole
    // group can be resized against the history list via the splitter handle.
    auto *outputSection = new QWidget(this);
    auto *outputLayout = new QVBoxLayout(outputSection);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(6);
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(m_summaryLabel, 1);
    headerLayout->addWidget(clearButton);
    outputLayout->addLayout(headerLayout);
    outputLayout->addWidget(m_outputText, 1);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName("commandOutputSplitter");
    splitter->addWidget(m_historyList);
    splitter->addWidget(outputSection);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({120, 300});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(splitter, 1);

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
    details += "$ " + commandLine + "\n\n";
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
