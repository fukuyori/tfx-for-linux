#include "PreviewPane.h"
#include "UiText.h"
#include "core/DelimitedText.h"
#include "core/PreviewText.h"
#include "platform/Platform.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextStream>
#include <QToolButton>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {
QIcon previewSourceIcon()
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#D9E1E8"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(6, 9, 16, 10));
    painter.setBrush(QColor("#D9E1E8"));
    painter.drawEllipse(QRectF(12, 12, 4, 4));
    return QIcon(pixmap);
}

QIcon previewExternalIcon()
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#D9E1E8"), 2);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    // Window frame with a corner left open for the out-going arrow.
    QPainterPath frame;
    frame.moveTo(15, 8);
    frame.lineTo(8, 8);
    frame.lineTo(8, 20);
    frame.lineTo(20, 20);
    frame.lineTo(20, 13);
    painter.drawPath(frame);
    // Arrow pointing out to the top-right.
    painter.drawLine(QPointF(14, 14), QPointF(21, 7));
    QPainterPath head;
    head.moveTo(15, 7);
    head.lineTo(21, 7);
    head.lineTo(21, 13);
    painter.drawPath(head);
    return QIcon(pixmap);
}

bool hasTextPreviewSuffix(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return QStringList({
        "md", "markdown", "mdown", "mkd",
        "html", "htm",
        "json", "xml", "js", "ts", "css",
        "csv", "tsv", "txt", "log", "ini", "toml", "yaml", "yml"
    }).contains(suffix);
}

QString imageDataUrl(const QString &path)
{
    QImageReader reader(path);
    if (!reader.canRead()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QMimeDatabase database;
    const QString mime = database.mimeTypeForFile(path).name();
    return QString("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(file.readAll().toBase64()));
}

QString cleanedMarkdownUrl(QString url)
{
    url = url.trimmed();
    if ((url.startsWith('<') && url.endsWith('>'))
        || (url.startsWith('"') && url.endsWith('"'))
        || (url.startsWith('\'') && url.endsWith('\''))) {
        url = url.mid(1, url.size() - 2);
    }
    return url;
}

bool isAllowedExternalPreviewUrl(const QUrl &url)
{
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https");
}

QString htmlPreBlock(const QString &content)
{
    QString escaped = content;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    return QString("<body><pre style='white-space:pre-wrap;'>%1</pre></body>")
        .arg(escaped);
}

bool isLocalPathUnderBase(const QString &path, const QDir &baseDir)
{
    const QString base = baseDir.canonicalPath();
    const QString target = QFileInfo(path).canonicalFilePath();
    if (base.isEmpty() || target.isEmpty()) {
        return false;
    }
    return target == base || target.startsWith(base + QLatin1Char('/'));
}

QString pdfPreviewCachePath(const QString &path)
{
    const QFileInfo info(path);
    const QString cacheRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath("pdf-preview");
    if (!QDir().mkpath(cacheRoot)) {
        return QString();
    }

    const QString identity = QString("%1|%2|%3")
        .arg(info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath())
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
    const QByteArray digest = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QDir(cacheRoot).filePath(QString::fromLatin1(digest) + ".png");
}

bool renderPdfPreviewWithFallback(const QString &path, const QString &outputPng)
{
    const QList<int> scales = {900, 720, 480};
    for (int scale : scales) {
        QFile::remove(outputPng);
        if (tfx::platform::renderPdfPreview(path, outputPng, scale) && QFileInfo::exists(outputPng)) {
            return true;
        }
    }
    return false;
}
}

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent),
      m_stack(new QStackedWidget(this)),
      m_title(new QLabel(this)),
      m_image(new QLabel(this)),
      m_text(new QPlainTextEdit(this)),
      m_rendered(new QTextBrowser(this)),
      m_sourceToggle(new QToolButton(this)),
      m_openExternal(new QToolButton(this))
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
    m_rendered->setOpenExternalLinks(false);
    m_rendered->setOpenLinks(false);
    connect(m_rendered, &QTextBrowser::anchorClicked, this, &PreviewPane::openPreviewLink);

    m_sourceToggle->setObjectName("previewSourceToggle");
    m_sourceToggle->setIcon(previewSourceIcon());
    m_sourceToggle->setIconSize(QSize(22, 22));
    m_sourceToggle->setCheckable(true);
    m_sourceToggle->setChecked(true);
    m_sourceToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(m_sourceToggle, &QToolButton::clicked, this, &PreviewPane::toggleSourceRendered);

    m_openExternal->setObjectName("previewOpenExternal");
    m_openExternal->setIcon(previewExternalIcon());
    m_openExternal->setIconSize(QSize(22, 22));
    m_openExternal->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_openExternal->setToolTip(UiText::t("Open in external viewer", "外部ビューアで開く"));
    m_openExternal->setVisible(false);
    connect(m_openExternal, &QToolButton::clicked, this, &PreviewPane::openCurrentPreviewExternally);

    m_stack->addWidget(m_text);
    m_stack->addWidget(m_image);
    m_stack->addWidget(m_rendered);

    auto *modeLayout = new QHBoxLayout();
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(6);
    modeLayout->addWidget(m_sourceToggle);
    modeLayout->addWidget(m_openExternal);
    modeLayout->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);
    layout->addLayout(modeLayout);
    layout->addWidget(m_title);
    layout->addWidget(m_stack, 1);
}

void PreviewPane::setPreviewConfig(const QString &defaultMode,
                                   const QHash<QString, QString> &extensionModes,
                                   const QString &markdownExternalImages)
{
    m_previewDefaultMode = defaultMode;
    m_previewExtensionModes = extensionModes;
    m_markdownExternalImages = markdownExternalImages;
}

void PreviewPane::previewPath(const QString &path)
{
    const QFileInfo info(path);
    m_currentImagePath.clear();
    m_externalPreviewUrl.clear();
    m_openExternal->setVisible(false);
    m_title->setVisible(true);
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

    // Preview mode from [preview.extensions] override, else [preview] default.
    const QString previewMode = m_previewExtensionModes.value(info.suffix().toLower(), m_previewDefaultMode);
    if (info.isFile() && previewMode == "none") {
        m_text->setPlainText(UiText::t("Content preview is disabled for this file type.",
                                       "このファイル形式はコンテンツプレビューが無効です。"));
        m_stack->setCurrentWidget(m_text);
        return;
    }
    if (info.isFile()) {
        if (previewMode == "text") {
            m_prefersRendered = false;
        } else if (previewMode == "rendered") {
            m_prefersRendered = true;
        }
        // "auto" keeps the current rendered/source preference.
    }

    if (info.isFile() && showImage(path)) {
        return;
    }
    if (info.isFile() && showPdf(path)) {
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

void PreviewPane::previewSelection(const QStringList &paths)
{
    m_currentImagePath.clear();
    m_externalPreviewUrl.clear();
    m_openExternal->setVisible(false);
    m_title->setVisible(true);
    setRenderAvailable(false);

    qint64 totalSize = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isFile()) {
            totalSize += info.size();
        }
    }
    m_title->setText(UiText::t("%1 items selected", "%1 件を選択").arg(paths.size()));

    QString body = UiText::t("Total size: %1 bytes\n\n", "合計サイズ: %1 bytes\n\n").arg(totalSize);
    const int limit = 50;
    for (int i = 0; i < paths.size() && i < limit; ++i) {
        const QFileInfo info(paths.at(i));
        const QString detail = info.isDir()
            ? UiText::t("Folder", "フォルダ")
            : QString::number(info.size());
        body += QString("%1\t%2\n").arg(info.fileName(), detail);
    }
    if (paths.size() > limit) {
        body += UiText::t("... and %1 more", "...ほか %1 件").arg(paths.size() - limit);
    }
    m_text->setPlainText(body);
    m_stack->setCurrentWidget(m_text);
}

void PreviewPane::openPreviewLink(const QUrl &url)
{
    if (!isAllowedExternalPreviewUrl(url)) {
        return;
    }
    QDesktopServices::openUrl(url);
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

QString PreviewPane::renderHtmlForTextFile(const QString &path, const QString &content)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "md" || suffix == "markdown" || suffix == "mdown" || suffix == "mkd") {
        return renderMarkdown(path, content);
    }
    if (suffix == "html" || suffix == "htm") {
        return htmlPreBlock(content);
    }
    if (suffix == "json") {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(content.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError) {
            return QString("<body><pre>%1</pre></body>")
                .arg(escapeHtml(QString::fromUtf8(document.toJson(QJsonDocument::Indented))));
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

QString PreviewPane::renderMarkdown(const QString &path, const QString &content)
{
    QString markdown = content;
    const QDir baseDir(QFileInfo(path).absolutePath());
    QRegularExpression imagePattern("!\\[([^\\]]*)\\]\\(([^\\)]*)\\)");
    QRegularExpressionMatchIterator iterator = imagePattern.globalMatch(content);
    QList<QPair<QString, QString>> replacements;

    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString rawTarget = match.captured(2);
        QString target = cleanedMarkdownUrl(rawTarget);
        const int titleStart = target.indexOf(QRegularExpression("\\s+['\"]"));
        if (titleStart > 0) {
            target = target.left(titleStart).trimmed();
        }
        const QUrl url(target);
        if (isAllowedExternalPreviewUrl(url)) {
            if (m_externalPreviewUrl.isEmpty()) {
                m_externalPreviewUrl = target;
            }
            // externalImages: "always" keeps the inline image; "never"/"button"
            // downgrade it to a link (blocked inline) with an open-externally
            // affordance via m_externalPreviewUrl.
            const QString replacement = m_markdownExternalImages == "always"
                ? QString("![%1](%2)").arg(match.captured(1), target)
                : QString("[%1](%2)").arg(match.captured(1), target);
            replacements.prepend({match.captured(0), replacement});
            continue;
        }
        if (!url.scheme().isEmpty() && url.scheme() != "file") {
            continue;
        }

        const QString localPath = url.isLocalFile()
            ? url.toLocalFile()
            : baseDir.filePath(QUrl::fromPercentEncoding(target.toUtf8()));
        if (!isLocalPathUnderBase(localPath, baseDir)) {
            continue;
        }
        const QString dataUrl = imageDataUrl(localPath);
        if (!dataUrl.isEmpty()) {
            replacements.prepend({match.captured(0), QString("![%1](%2)").arg(match.captured(1), dataUrl)});
        }
    }

    for (const auto &replacement : replacements) {
        markdown.replace(replacement.first, replacement.second);
    }

    const QString body = QTextDocumentFragment::fromMarkdown(
        markdown,
        QTextDocument::MarkdownDialectGitHub).toHtml();
    return QString("<body style='font-family:sans-serif;line-height:1.5;'>%1</body>")
        .arg(body);
}

QString PreviewPane::csvToHtmlTable(const QString &content, QChar delimiter) const
{
    constexpr int MaxRows = 1000;
    constexpr int MaxColumns = 100;
    const tfx::core::DelimitedTable table = tfx::core::parseDelimited(content, delimiter, MaxRows, MaxColumns);

    QString html = "<body><table cellspacing='0' cellpadding='6' style='border-collapse:collapse;font-family:monospace;'>";
    for (const QStringList &row : table.rows) {
        html += "<tr>";
        for (const QString &cell : row) {
            html += QString("<td style='border:1px solid #2a333a;'>%1</td>").arg(escapeHtml(cell));
        }
        html += "</tr>";
    }
    html += "</table>";
    if (table.rowsTruncated || table.columnsTruncated) {
        html += QString("<p style='font-family:sans-serif;'>%1</p>")
            .arg(escapeHtml(UiText::t("Preview limited to %1 rows × %2 columns.",
                                      "プレビューは %1 行 × %2 列までに制限されています。")
                                .arg(MaxRows)
                                .arg(MaxColumns)));
    }
    html += "</body>";
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
    m_currentImagePath = path;
    m_openExternal->setVisible(true);
    return true;
}

bool PreviewPane::showPdf(const QString &path)
{
    if (QFileInfo(path).suffix().toLower() != "pdf") {
        return false;
    }

    if (!tfx::platform::pdfPreviewAvailable()) {
        m_text->setPlainText(UiText::t("PDF preview is unavailable on this system.", "この環境では PDF プレビューを利用できません。"));
        setRenderAvailable(false);
        m_stack->setCurrentWidget(m_text);
        return true;
    }

    QTemporaryDir tempDir;
    QString outputPng = pdfPreviewCachePath(path);
    if (outputPng.isEmpty() || !QFileInfo::exists(outputPng)) {
        if (!tempDir.isValid()) {
            return false;
        }

        const QString tempPng = tempDir.filePath("preview.png");
        if (!renderPdfPreviewWithFallback(path, tempPng)) {
            m_text->setPlainText(UiText::t("Could not render PDF preview.", "PDF プレビューを作成できませんでした。"));
            setRenderAvailable(false);
            m_stack->setCurrentWidget(m_text);
            return true;
        }
        if (outputPng.isEmpty()) {
            outputPng = tempPng;
        } else {
            QFile::remove(outputPng);
            if (!QFile::copy(tempPng, outputPng)) {
                outputPng = tempPng;
            }
        }
    }

    const QPixmap pixmap(outputPng);
    if (pixmap.isNull()) {
        return false;
    }

    setRenderAvailable(false);
    m_image->setPixmap(pixmap.scaled(720, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
        || type.name().contains("x-shellscript")
        || hasTextPreviewSuffix(path);
    if (!likelyText) {
        return false;
    }

    // Shared size-capped loader: large files preview their head instead of
    // being read whole into memory.
    constexpr qint64 TextPreviewCapBytes = 4 * 1024 * 1024;
    const tfx::core::LoadedText loaded = tfx::core::loadTextCapped(path, TextPreviewCapBytes);
    if (!loaded.ok) {
        return false;
    }
    const QString &content = loaded.text;
    if (loaded.truncated) {
        m_text->setPlainText(content
                             + UiText::t("\n… (preview truncated at 4 MB)",
                                         "\n… （プレビューは 4 MB で打ち切られました）"));
    } else {
        m_text->setPlainText(content);
    }

    const QString renderedHtml = renderHtmlForTextFile(path, content);
    setRenderAvailable(!renderedHtml.isEmpty());
    if (!renderedHtml.isEmpty()) {
        m_rendered->document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath() + "/"));
        m_rendered->setHtml(renderedHtml);
        m_openExternal->setVisible(!m_externalPreviewUrl.isEmpty());
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
    m_title->setVisible(m_stack->currentWidget() != m_rendered);
    m_sourceToggle->setChecked(m_stack->currentWidget() == m_rendered && m_renderAvailable);
    m_sourceToggle->setToolTip(m_stack->currentWidget() == m_text
        ? UiText::t("Show rendered preview", "レンダリング表示")
        : UiText::t("Show source", "ソースを表示"));
}

void PreviewPane::toggleSourceRendered()
{
    if (!m_renderAvailable) {
        return;
    }
    m_prefersRendered = !m_prefersRendered;
    showPreferredTextView();
}

void PreviewPane::openCurrentPreviewExternally()
{
    if (!m_externalPreviewUrl.isEmpty()) {
        const QUrl url(m_externalPreviewUrl);
        if (isAllowedExternalPreviewUrl(url)) {
            QDesktopServices::openUrl(url);
        }
        return;
    }
    if (m_currentImagePath.isEmpty()) {
        return;
    }
    tfx::platform::openPath(m_currentImagePath);
}
