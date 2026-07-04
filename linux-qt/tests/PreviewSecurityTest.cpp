#include "PreviewPane.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextStream>

namespace {
bool writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << text;
    return true;
}

QString renderedHtml(PreviewPane *pane)
{
    auto *browser = pane->findChild<QTextBrowser *>(QStringLiteral("previewRendered"));
    return browser ? browser->toHtml() : QString();
}
}

class PreviewSecurityTest : public QObject
{
    Q_OBJECT

private slots:
    void htmlFilesRenderAsEscapedSource();
    void remoteMarkdownImagesRenderAsLinks();
};

void PreviewSecurityTest::htmlFilesRenderAsEscapedSource()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath("sample.html");
    QVERIFY(writeTextFile(path, "<script>alert(1)</script>"));

    PreviewPane pane;
    pane.previewPath(path);

    const QString html = renderedHtml(&pane);
    QVERIFY(html.contains("&lt;script&gt;alert(1)&lt;/script&gt;"));
    QVERIFY(!html.contains("<script>alert(1)</script>"));
}

void PreviewSecurityTest::remoteMarkdownImagesRenderAsLinks()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath("sample.md");
    QVERIFY(writeTextFile(path, "![remote](https://example.com/image.png)"));

    PreviewPane pane;
    pane.previewPath(path);

    const QString html = renderedHtml(&pane);
    QVERIFY(!html.contains("<img"));
    QVERIFY(html.contains("https://example.com/image.png"));
}

QTEST_MAIN(PreviewSecurityTest)

#include "PreviewSecurityTest.moc"
