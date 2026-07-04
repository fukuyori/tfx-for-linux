#pragma once

#include <QLabel>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QWidget>

class PreviewPane : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPane(QWidget *parent = nullptr);

public slots:
    void previewPath(const QString &path);
    void previewSelection(const QStringList &paths);
    void toggleSourceRendered();
    void openCurrentPreviewExternally();

private:
    QString metadataText(const QFileInfo &info) const;
    QString renderHtmlForTextFile(const QString &path, const QString &content);
    QString renderMarkdown(const QString &path, const QString &content);
    QString csvToHtmlTable(const QString &content, QChar delimiter) const;
    QString escapeHtml(const QString &text) const;
    bool showImage(const QString &path);
    bool showPdf(const QString &path);
    bool showText(const QString &path);
    void setRenderAvailable(bool available);
    void showPreferredTextView();

    QStackedWidget *m_stack;
    QLabel *m_title;
    QLabel *m_image;
    QPlainTextEdit *m_text;
    QTextBrowser *m_rendered;
    QToolButton *m_sourceToggle;
    QToolButton *m_openExternal;
    QString m_currentImagePath;
    QString m_externalPreviewUrl;
    bool m_renderAvailable = false;
    bool m_prefersRendered = true;
};
