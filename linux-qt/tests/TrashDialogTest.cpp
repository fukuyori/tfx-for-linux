#include "TrashDialog.h"
#include "AppConfig.h"
#include "UiText.h"
#include <QAction>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMenu>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TrashDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void listsAndRestoresSelectedItem_data()
    {
        QTest::addColumn<bool>("embedded");
        QTest::newRow("window") << false;
        QTest::newRow("file-area") << true;
    }
    void listsAndRestoresSelectedItem()
    {
        QFETCH(bool, embedded);
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
        const auto restoreEnvironment = qScopeGuard([oldDataHome]() {
            if (oldDataHome.isNull()) qunsetenv("XDG_DATA_HOME");
            else qputenv("XDG_DATA_HOME", oldDataHome);
        });
        qputenv("XDG_DATA_HOME", QFile::encodeName(tmp.path()));
        const QString root = tmp.path() + "/Trash";
        QVERIFY(QDir().mkpath(root + "/files"));
        QVERIFY(QDir().mkpath(root + "/info"));
        for (const QString &dir : {root, root + "/files", root + "/info"})
            QVERIFY(QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QString destination = tmp.path() + "/restored.txt";
        QFile file(root + "/files/test-item");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test content");
        file.close();
        QFile metadata(root + "/info/test-item.trashinfo");
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write("[Trash Info]\nPath=" + QFile::encodeName(destination).toPercentEncoding("/")
                       + "\nDeletionDate=2026-09-09T12:34:56\n");
        metadata.close();

        QWidget container;
        container.resize(900, 550);
        // Simulate the generic sidebar rule inherited from the application.
        container.setStyleSheet("QTreeView { background: #112233; color: #445566; }");
        TrashDialog dialog(embedded ? &container : nullptr, embedded);
        AppColors colors;
        colors.panelBackground = "#203040";
        colors.selectedBackground = "#964321";
        colors.selectedForeground = "#ffffff";
        QFont font;
        font.setPixelSize(15);
        dialog.setFileListAppearance(colors, font);
        if (embedded) container.show();
        dialog.show();
        QCOMPARE(dialog.isWindow(), !embedded);
        auto *list = dialog.findChild<QTreeWidget *>("trashList");
        auto *restore = dialog.findChild<QPushButton *>("restoreTrashButton");
        QVERIFY(list);
        QVERIFY(restore);
        QCOMPARE(restore->text(), UiText::t("Restore to Original Location", "元の場所へ復元"));
        QCOMPARE(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->text(),
                 UiText::t("Close", "閉じる"));
        QVERIFY(!restore->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!list->findItems(destination, Qt::MatchExactly, 1).isEmpty(), 10000);
        // Entries without metadata can be permanently deleted, but not restored.
        QFile orphan(root + "/files/orphan");
        QVERIFY(orphan.open(QIODevice::WriteOnly));
        orphan.write("orphan");
        orphan.close();
        dialog.refresh();
        QTRY_VERIFY(!list->findItems("orphan", Qt::MatchExactly, 0).isEmpty());
        auto *orphanItem = list->findItems("orphan", Qt::MatchExactly, 0).first();
        const QPoint point = list->visualItemRect(orphanItem).center();
        for (const auto answer : {QMessageBox::No, QMessageBox::Yes}) {
            QTimer::singleShot(0, &dialog, [&dialog, answer]() {
                auto *menu = dialog.findChild<QMenu *>();
                QVERIFY(menu);
                auto *restoreAction = menu->findChild<QAction *>("restoreTrashAction");
                auto *deleteAction = menu->findChild<QAction *>("deleteTrashAction");
                QVERIFY(restoreAction);
                QVERIFY(deleteAction);
                QVERIFY(!restoreAction->isEnabled());
                menu->close();
                QTimer::singleShot(0, &dialog, [answer]() {
                    auto *question = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                    QVERIFY(question);
                    QCOMPARE(question->defaultButton(), question->button(QMessageBox::No));
                    question->button(answer)->click();
                });
                deleteAction->trigger();
            });
            list->customContextMenuRequested(point);
            if (answer == QMessageBox::No) QVERIFY(QFileInfo::exists(orphan.fileName()));
        }
        QTRY_VERIFY(!QFileInfo::exists(orphan.fileName()));
        QTRY_VERIFY(list->findItems("orphan", Qt::MatchExactly, 0).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!list->findItems(destination, Qt::MatchExactly, 1).isEmpty(), 10000);
        auto *item = list->findItems(destination, Qt::MatchExactly, 1).first();
        QCOMPARE(item->text(0), QString("restored.txt"));
        QVERIFY(!item->icon(0).isNull());
        item->setSelected(true);
        QVERIFY(restore->isEnabled());
        list->setFocus();
        const auto selectedColor = [&]() {
            const QRect rect = list->visualItemRect(item);
            return list->viewport()->grab().toImage().pixelColor(list->viewport()->width() - 12, rect.center().y());
        };
        QTRY_COMPARE(selectedColor(), QColor(colors.selectedBackground));
        restore->setFocus();
        QTRY_COMPARE(selectedColor(), QColor(colors.selectedBackground));
        QSignalSpy restored(&dialog, &TrashDialog::restored);
        QTimer::singleShot(0, &dialog, []() {
            auto *question = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (question) question->button(QMessageBox::Yes)->click();
        });
        restore->click();
        QCOMPARE(restored.count(), 1);
        QVERIFY(QFileInfo::exists(destination));
        QTRY_VERIFY(list->findItems(destination, Qt::MatchExactly, 1).isEmpty());
        QVERIFY(!restore->isEnabled());
    }
};

QTEST_MAIN(TrashDialogTest)
#include "TrashDialogTest.moc"
