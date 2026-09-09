#include "views/FileIcons.h"
#include <QtTest>

using namespace tfx::views;

class FileIconsTest : public QObject
{
    Q_OBJECT
private slots:
    void classifiesFileNames()
    {
        QCOMPARE(fileIconKind("PHOTO.PNG"), FileIconKind::Image);
        QCOMPARE(fileIconKind("report.pdf"), FileIconKind::Pdf);
        QCOMPARE(fileIconKind("source.rs"), FileIconKind::Code);
        QCOMPARE(fileIconKind("data.csv"), FileIconKind::Spreadsheet);
        QCOMPARE(fileIconKind("archive.tar.gz"), FileIconKind::Archive);
        QCOMPARE(fileIconKind("/tmp/photo.png/unknown"), FileIconKind::File);
        QCOMPARE(fileIconKind("unknown.extension"), FileIconKind::File);
        // Preserve prism-fm's first-match priority for overlapping extensions.
        QCOMPARE(fileIconKind("drawing.svg"), FileIconKind::Image);
        QCOMPARE(fileIconKind("clip.ts"), FileIconKind::Video);
        QCOMPARE(fileIconKind("slides.key"), FileIconKind::Presentation);
    }

    void rendersDistinctCategoriesAndCachesByColor()
    {
        const QStringList names{"unknown", "photo.png", "clip.mp4", "music.flac", "report.pdf",
            "table.csv", "slides.pptx", "document.docx", "notes.txt", "source.cpp", "markup.xml",
            "data.toml", "script.sh", "database.db", "font.ttf", "archive.zip", "disk.img",
            "program.exe", "library.so", "certificate.pem", "model.stl"};
        QSet<QByteArray> rendered;
        for (const QString &name : names) {
            const auto icon = fileIcon(name, QColor("#abcdef"));
            QVERIFY(!icon.isNull());
            QCOMPARE(icon.cacheKey(), fileIcon(name, QColor("#abcdef")).cacheKey());
            const QImage bitmap = icon.pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
            const QByteArray pixels(reinterpret_cast<const char *>(bitmap.constBits()), bitmap.sizeInBytes());
            QVERIFY(!rendered.contains(pixels));
            rendered.insert(pixels);
        }
        QVERIFY(fileIcon("test.pdf", Qt::red).pixmap(32).toImage()
                != fileIcon("test.pdf", Qt::blue).pixmap(32).toImage());
    }
};
QTEST_MAIN(FileIconsTest)
#include "FileIconsTest.moc"
