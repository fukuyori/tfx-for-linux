#include "QuickPreviewOverlay.h"
#include "PreviewPane.h"
#include "UiText.h"
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest>

class QuickPreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void keyboardAndContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("note.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("quick preview content");
        file.close();
        QWidget host;
        host.resize(800, 600);
        QuickPreviewOverlay overlay(&host);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        overlay.setNavigationView(&host);
        host.setFocus();
        overlay.present({file.fileName()});
        QVERIFY(overlay.isVisible());
        QCOMPARE(overlay.previewPane()->findChild<QPlainTextEdit *>()->toPlainText(),
                 QString("quick preview content"));
        QVERIFY(!overlay.previewPane()->findChild<QLabel *>("previewTitle")->isVisible());
        QTest::keyClick(&host, Qt::Key_Space);
        QVERIFY(!overlay.isVisible());
        overlay.present({file.fileName()});
        QVERIFY(overlay.isVisible());
        QTest::keyClick(&host, Qt::Key_Escape);
        QVERIFY(!overlay.isVisible());
        QSignalSpy open(&overlay, &QuickPreviewOverlay::openRequested);
        overlay.present({file.fileName()});
        QTest::keyClick(&host, Qt::Key_Return);
        QCOMPARE(open.count(), 1);
        QVERIFY(!overlay.isVisible());
        overlay.setShortcut(QKeySequence(Qt::Key_F3));
        overlay.present({file.fileName()});
        QTest::keyClick(&host, Qt::Key_F3);
        QVERIFY(!overlay.isVisible());
    }
    void keysStayWithOtherInputs()
    {
        QTemporaryDir dir;
        QWidget host;
        host.resize(800, 600);
        QWidget navigation(&host);
        navigation.setFocusPolicy(Qt::StrongFocus);
        QLineEdit search(&host);
        QPlainTextEdit terminal(&host);
        QLineEdit renameEditor(&navigation);
        QuickPreviewOverlay overlay(&host);
        overlay.setNavigationView(&navigation);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        navigation.setFocus();
        overlay.present({dir.path()});
        QSignalSpy open(&overlay, &QuickPreviewOverlay::openRequested);
        for (QLineEdit *input : {&search, &renameEditor}) {
            input->setFocus();
            QSignalSpy submit(input, &QLineEdit::returnPressed);
            QTest::keyClicks(input, "one two");
            QTest::keyClick(input, Qt::Key_Return);
            QTest::keyClick(input, Qt::Key_Escape);
            QCOMPARE(input->text(), QString("one two"));
            QCOMPARE(submit.count(), 1);
            QCOMPARE(QApplication::focusWidget(), input);
            QVERIFY(overlay.isVisible());
        }
        terminal.setFocus();
        QTest::keyClicks(&terminal, "echo hello");
        QTest::keyClick(&terminal, Qt::Key_Return);
        QCOMPARE(terminal.toPlainText(), QString("echo hello\n"));
        QCOMPARE(open.count(), 0);
        QVERIFY(overlay.isVisible());
        navigation.setFocus();
        QTest::keyClick(&navigation, Qt::Key_Escape);
        QVERIFY(!overlay.isVisible());
    }

    void listingDoesNotChangeHiddenFilePreview()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("note.md"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# heading");
        file.close();
        QWidget host;
        host.resize(800, 600);
        QuickPreviewOverlay overlay(&host);
        host.show();
        overlay.present({file.fileName()});
        auto *source = overlay.previewPane()->findChild<QPlainTextEdit *>();
        QVERIFY(!source->isVisible());
        overlay.previewPaths({dir.path()});
        overlay.toggleSourceRendered();
        overlay.previewPaths({file.fileName()});
        // Toggling while the listing was visible must not silently change the
        // preference of the hidden preview, which is reused for the next file.
        QVERIFY(!source->isVisible());
        overlay.toggleSourceRendered();
        QVERIFY(source->isVisible());
    }

    void folderAndMultipleSelection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath("nested/child"));
        QFile hidden(dir.filePath(".secret"));
        QVERIFY(hidden.open(QIODevice::WriteOnly));
        QWidget host;
        host.resize(800, 600);
        QuickPreviewOverlay overlay(&host);
        host.show();
        overlay.present({dir.path()});
        QPlainTextEdit *listing = nullptr;
        const auto editors = overlay.findChildren<QPlainTextEdit *>();
        for (auto *editor : editors) {
            if (!overlay.previewPane()->isAncestorOf(editor)) listing = editor;
        }
        QVERIFY(listing);
        QTRY_VERIFY(listing->toPlainText().contains("child/"));
        QVERIFY(!listing->toPlainText().contains(".secret"));
        overlay.previewPaths({dir.filePath("nested"), hidden.fileName()});
        QCOMPARE(listing->toPlainText(), QString("nested/\n.secret"));
        overlay.previewPaths({dir.filePath("nested/child")});
        QTRY_COMPARE(listing->toPlainText(), UiText::t("(empty folder)", "(空のフォルダ)"));
        QSignalSpy dismissed(&overlay, &QuickPreviewOverlay::dismissed);
        overlay.previewPaths({});
        QVERIFY(overlay.isVisible());
        QCOMPARE(listing->toPlainText(), UiText::t("Nothing selected.", "選択なし"));
        QCOMPARE(dismissed.count(), 0);
        overlay.dismiss();
        overlay.dismiss();
        QCOMPARE(dismissed.count(), 1);
    }
};
QTEST_MAIN(QuickPreviewTest)
#include "QuickPreviewTest.moc"
