#include "PreviewPane.h"
#include "UiText.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QTextStream>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {
QIcon previewSourceIcon()
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#D7D7D7"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(6, 9, 16, 10));
    painter.setBrush(QColor("#D7D7D7"));
    painter.drawEllipse(QRectF(12, 12, 4, 4));
    return QIcon(pixmap);
}
}

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent),
      m_stack(new QStackedWidget(this)),
      m_title(new QLabel(this)),
      m_image(new QLabel(this)),
      m_text(new QPlainTextEdit(this)),
      m_rendered(new QTextBrowser(this)),
      m_sourceToggle(new QToolButton(this))
{
    setObjectName("previewPane");
    setMinimumWidth(240);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_title->setWordWrap(true);
    m_title->setObjectName("previewTitle");
    m_image->setAlignment(Qt::AlignCenter);
    m_image->setScaledContents(false);
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setObjectName("previewCode");
    m_rendered->setObjectName("previewRendered");
    m_rendered->setOpenExternalLinks(true);

    m_sourceToggle->setObjectName("previewSourceToggle");
    m_sourceToggle->setIcon(previewSourceIcon());
    m_sourceToggle->setIconSize(QSize(22, 22));
    m_sourceToggle->setCheckable(true);
    m_sourceToggle->setChecked(true);
    m_sourceToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(m_sourceToggle, &QToolButton::clicked, this, [this]() {
        m_prefersRendered = !m_prefersRendered;
        showPreferredTextView();
    });

    m_stack->addWidget(m_text);
    m_stack->addWidget(m_image);
    m_stack->addWidget(m_rendered);

    auto *modeLayout = new QHBoxLayout();
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->addStretch(1);
    modeLayout->addWidget(m_sourceToggle);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);
    layout->addWidget(m_title);
    layout->addLayout(modeLayout);
    layout->addWidget(m_stack, 1);
}

void PreviewPane::previewPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        m_title->setText(UiText::t("No selection", "選択なし"));
        m_text->clear();
        m_rendered->clear();
        setRenderAvailable(false);
        m_stack->setCurrentWidget(m_text);
        return;
    }

    m_title->setText(metadataText(info));
    setRenderAvailable(false);
    if (info.isFile() && showImage(path)) {
        return;
    }
    if (info.isFile() && showText(path)) {
        return;
    }
    m_text->setPlainText(info.isDir()
        ? UiText::t("Directory", "フォルダ")
        : UiText::t("Preview is not available for this file type.", "このファイル形式はプレビューできません。"));
    m_stack->setCurrentWidget(m_text);
}

QString PreviewPane::metadataText(const QFileInfo &info) const
{
    return UiText::t("%1\n%2\nSize: %3 bytes\nModified: %4\nPath: %5",
                     "%1\n%2\nサイズ: %3 bytes\n更新日時: %4\nパス: %5")
        .arg(info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName())
        .arg(info.isDir() ? UiText::t("Folder", "フォルダ") : UiText::t("File", "ファイル"))
        .arg(info.size())
        .arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"))
        .arg(info.absoluteFilePath());
}

QString PreviewPane::renderHtmlForTextFile(const QString &path, const QString &content) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "md" || suffix == "markdown" || suffix == "mdown" || suffix == "mkd") {
        return QString("<body style='background:#000;color:#ddd;font-family:sans-serif;'>%1</body>")
            .arg(QTextDocumentFragment::fromMarkdown(content).toHtml());
    }
    if (suffix == "html" || suffix == "htm") {
        return content;
    }
    if (suffix == "json") {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(content.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError) {
            return QString("<pre>%1</pre>").arg(escapeHtml(QString::fromUtf8(document.toJson(QJsonDocument::Indented))));
        }
    }
    if (suffix == "csv") {
        return csvToHtmlTable(content, ',');
    }
    if (suffix == "tsv") {
        return csvToHtmlTable(content, '\t');
    }
    return {};
}

QString PreviewPane::csvToHtmlTable(const QString &content, QChar delimiter) const
{
    QString html = "<table cellspacing='0' cellpadding='6' style='border-collapse:collapse;color:#ddd;font-family:monospace;'>";
    const QStringList lines = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString &line : lines.mid(0, 200)) {
        html += "<tr>";
        const QStringList cells = line.split(delimiter);
        for (const QString &cell : cells) {
            html += QString("<td style='border:1px solid #333;'>%1</td>").arg(escapeHtml(cell));
        }
        html += "</tr>";
    }
    html += "</table>";
    return html;
}

QString PreviewPane::escapeHtml(const QString &text) const
{
    QString escaped = text;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    return escaped;
}

bool PreviewPane::showImage(const QString &path)
{
    QImageReader reader(path);
    if (!reader.canRead()) {
        return false;
    }
    QPixmap pixmap(path);
    if (pixmap.isNull()) {
        return false;
    }
    setRenderAvailable(false);
    m_image->setPixmap(pixmap.scaled(520, 520, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_stack->setCurrentWidget(m_image);
    return true;
}

bool PreviewPane::showText(const QString &path)
{
    QMimeDatabase database;
    const QMimeType type = database.mimeTypeForFile(path);
    const bool likelyText = type.name().startsWith("text/")
        || type.name().contains("json")
        || type.name().contains("xml")
        || type.name().contains("javascript")
        || type.name().contains("x-shellscript");
    if (!likelyText) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray data = file.read(256 * 1024);
    const QString content = QString::fromUtf8(data);
    m_text->setPlainText(content);

    const QString renderedHtml = renderHtmlForTextFile(path, content);
    setRenderAvailable(!renderedHtml.isEmpty());
    if (!renderedHtml.isEmpty()) {
        m_rendered->setHtml(renderedHtml);
    } else {
        m_rendered->clear();
    }
    showPreferredTextView();
    return true;
}

void PreviewPane::setRenderAvailable(bool available)
{
    m_renderAvailable = available;
    m_sourceToggle->setVisible(available);
    m_sourceToggle->setEnabled(available);
    m_sourceToggle->setChecked(m_prefersRendered && available);
    m_sourceToggle->setToolTip(m_prefersRendered
        ? UiText::t("Show source", "ソースを表示")
        : UiText::t("Show rendered preview", "レンダリング表示"));
}

void PreviewPane::showPreferredTextView()
{
    if (m_prefersRendered && m_renderAvailable) {
        m_stack->setCurrentWidget(m_rendered);
    } else {
        m_stack->setCurrentWidget(m_text);
    }
    m_sourceToggle->setChecked(m_stack->currentWidget() == m_rendered && m_renderAvailable);
    m_sourceToggle->setToolTip(m_stack->currentWidget() == m_text
        ? UiText::t("Show rendered preview", "レンダリング表示")
        : UiText::t("Show source", "ソースを表示"));
}
