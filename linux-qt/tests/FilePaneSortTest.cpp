#include "FilePane.h"
#include "core/SortOptions.h"
#include "models/FileColumns.h"
#include "views/SortOptionsDialog.h"

#include <QHeaderView>
#include <QListWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

using namespace tfx::core;
using tfx::views::SortOptionsDialog;

// Sorting as the user drives it: clicking a column title in the real pane, and
// the keyboard-driven Sort Options popup behind the shortcut.
class FilePaneSortTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void clickingHeaderSortsAndTogglesDirection();
    void clickingAnotherHeaderSortsByThatColumn();
    void headerTitleMarksTheSortedColumnOnly();
    void parentEntryStaysFirstInBothDirections();
    void naturalKeySortsEmbeddedNumbersByValue();
    void typeKeySortsByTypeRatherThanTheSourceColumn();
    void synthesisedColumnsSortRatherThanBeingIgnored();
    void dialogNavigatesAndTogglesOrder();
    void dialogDigitJumpsAndEnterAccepts();
    void savedSortIsRestoredOnANewPane();
    void headerClickSticksWhenAColumnLayoutIsSaved();
    void parentEntryShowsNoMetadata();

private:
    QTableView *fileView() const;
    QTableView *fileViewOf(FilePane *pane) const;
    QStringList visibleNames() const;
    QStringList namesOf(FilePane *pane) const;
    FilePane *openPane(const QString &label);
    void clickHeader(int column);

    QTemporaryDir *m_dir = nullptr;
    FilePane *m_pane = nullptr;
};

void FilePaneSortTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void FilePaneSortTest::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());

    // Names whose plain and natural orderings differ, with sizes deliberately
    // opposed to the name order so a size sort cannot pass by accident.
    const QVector<QPair<QString, int>> files = {
        {"file2.txt", 300},
        {"file10.txt", 200},
        {"file1.txt", 100},
    };
    // Distinct modification times, ordered opposite to the names, so a sort by
    // Date Modified cannot coincide with a name or size ordering.
    const QDateTime base = QDateTime::currentDateTime().addDays(-1);
    int ageHours = 0;
    for (const auto &entry : files) {
        QFile file(m_dir->filePath(entry.first));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QByteArray(entry.second, 'x')) == entry.second);
        // Closing first: flushing buffered data on close would otherwise stamp
        // the modification time again and undo the value set here.
        file.close();
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(base.addSecs(3600 * ageHours++), QFileDevice::FileModificationTime));
        file.close();
    }
    QVERIFY(QDir(m_dir->path()).mkdir("folder1"));

    m_pane = new FilePane("TEST", m_dir->path());
    m_pane->resize(900, 600);
    m_pane->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_pane));

    // QFileSystemModel populates asynchronously; the list also shows a ".."
    // parent row.
    QTRY_VERIFY_WITH_TIMEOUT(
        fileView()->model()->rowCount(fileView()->rootIndex()) >= 5, 5000);
    QTest::qWait(50);
}

void FilePaneSortTest::cleanup()
{
    delete m_pane;
    m_pane = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

QTableView *FilePaneSortTest::fileView() const
{
    return fileViewOf(m_pane);
}

QTableView *FilePaneSortTest::fileViewOf(FilePane *pane) const
{
    // Several table views share the "fileTable" object name; the file list is
    // the one visible in the pane's view stack.
    const QList<QTableView *> views = pane->findChildren<QTableView *>("fileTable");
    for (QTableView *view : views) {
        if (view->isVisible()) {
            return view;
        }
    }
    return nullptr;
}

FilePane *FilePaneSortTest::openPane(const QString &label)
{
    auto *pane = new FilePane(label, m_dir->path());
    pane->resize(900, 600);
    pane->show();
    if (!QTest::qWaitForWindowExposed(pane)) {
        return pane;
    }
    QTableView *view = fileViewOf(pane);
    if (view) {
        QTest::qWait(150);
    }
    return pane;
}

QStringList FilePaneSortTest::namesOf(FilePane *pane) const
{
    QStringList names;
    QTableView *view = fileViewOf(pane);
    QAbstractItemModel *model = view->model();
    const QModelIndex root = view->rootIndex();
    for (int row = 0; row < model->rowCount(root); ++row) {
        names.append(model->index(row, ColumnName, root).data().toString());
    }
    names.removeAll("..");
    names.removeAll("folder1");
    return names;
}

QStringList FilePaneSortTest::visibleNames() const
{
    QStringList names;
    QAbstractItemModel *model = fileView()->model();
    const QModelIndex root = fileView()->rootIndex();
    for (int row = 0; row < model->rowCount(root); ++row) {
        names.append(model->index(row, ColumnName, root).data().toString());
    }
    return names;
}

void FilePaneSortTest::clickHeader(int column)
{
    QHeaderView *header = fileView()->horizontalHeader();
    const int x = header->sectionViewportPosition(column) + header->sectionSize(column) / 2;
    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(x, header->height() / 2));
    QTest::qWait(20);
}

void FilePaneSortTest::clickingHeaderSortsAndTogglesDirection()
{
    QHeaderView *header = fileView()->horizontalHeader();
    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);
    const QStringList ascending = visibleNames();

    clickHeader(ColumnName);
    QCOMPARE(header->sortIndicatorSection(), static_cast<int>(ColumnName));
    QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);

    QStringList reversed = ascending;
    std::reverse(reversed.begin(), reversed.end());
    // ".." is pinned to the top rather than reversed with the rest.
    reversed.removeAll("..");
    reversed.prepend("..");
    QCOMPARE(visibleNames(), reversed);

    clickHeader(ColumnName);
    QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);
    QCOMPARE(visibleNames(), ascending);
}

void FilePaneSortTest::clickingAnotherHeaderSortsByThatColumn()
{
    QHeaderView *header = fileView()->horizontalHeader();
    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);

    clickHeader(ColumnSize);
    QCOMPARE(header->sortIndicatorSection(), static_cast<int>(ColumnSize));
    QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);

    QStringList names = visibleNames();
    names.removeAll("..");
    names.removeAll("folder1");
    // Ascending by size: 100, 200, 300 bytes.
    QCOMPARE(names, QStringList({"file1.txt", "file10.txt", "file2.txt"}));
}

void FilePaneSortTest::headerTitleMarksTheSortedColumnOnly()
{
    QAbstractItemModel *model = fileView()->model();
    m_pane->applySortKey(SortKey::Size, Qt::DescendingOrder);

    const QString sorted = model->headerData(ColumnSize, Qt::Horizontal).toString();
    QVERIFY2(sorted.contains(QChar(0x25bc)), qPrintable(sorted));
    QVERIFY(!model->headerData(ColumnName, Qt::Horizontal).toString().contains(QChar(0x25bc)));
    QVERIFY(!model->headerData(ColumnName, Qt::Horizontal).toString().contains(QChar(0x25b2)));

    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);
    QVERIFY(model->headerData(ColumnName, Qt::Horizontal).toString().contains(QChar(0x25b2)));
    QVERIFY(!model->headerData(ColumnSize, Qt::Horizontal).toString().contains(QChar(0x25bc)));
}

void FilePaneSortTest::parentEntryStaysFirstInBothDirections()
{
    for (const SortKey key : {SortKey::Name, SortKey::Size, SortKey::DateModified, SortKey::Type}) {
        for (const Qt::SortOrder order : {Qt::AscendingOrder, Qt::DescendingOrder}) {
            m_pane->applySortKey(key, order);
            QCOMPARE(visibleNames().value(0), QString(".."));
        }
    }
}

void FilePaneSortTest::naturalKeySortsEmbeddedNumbersByValue()
{
    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);
    QStringList plain = visibleNames();
    plain.removeAll("..");
    plain.removeAll("folder1");
    QCOMPARE(plain, QStringList({"file1.txt", "file10.txt", "file2.txt"}));

    m_pane->applySortKey(SortKey::Natural, Qt::AscendingOrder);
    QStringList natural = visibleNames();
    natural.removeAll("..");
    natural.removeAll("folder1");
    QCOMPARE(natural, QStringList({"file1.txt", "file2.txt", "file10.txt"}));
}

void FilePaneSortTest::typeKeySortsByTypeRatherThanTheSourceColumn()
{
    m_pane->applySortKey(SortKey::Type, Qt::AscendingOrder);

    QAbstractItemModel *model = fileView()->model();
    const QModelIndex root = fileView()->rootIndex();
    QStringList types;
    for (int row = 0; row < model->rowCount(root); ++row) {
        if (model->index(row, ColumnName, root).data().toString() == "..") {
            continue;
        }
        types.append(model->index(row, ColumnType, root).data().toString());
    }

    QVERIFY(types.size() >= 4);
    QStringList sorted = types;
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    QCOMPARE(types, sorted);
}

void FilePaneSortTest::synthesisedColumnsSortRatherThanBeingIgnored()
{
    // Date Modified, File Mode and Git Status sit past the columns the source
    // model provides. Qt cannot resolve a source sort column for them, so
    // clicking those headers used to leave the list untouched.
    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);

    m_pane->applySortKey(SortKey::DateModified, Qt::AscendingOrder);
    QStringList oldestFirst = visibleNames();
    oldestFirst.removeAll("..");
    oldestFirst.removeAll("folder1");
    QCOMPARE(oldestFirst, QStringList({"file2.txt", "file10.txt", "file1.txt"}));

    m_pane->applySortKey(SortKey::DateModified, Qt::DescendingOrder);
    QStringList newestFirst = visibleNames();
    newestFirst.removeAll("..");
    newestFirst.removeAll("folder1");
    QCOMPARE(newestFirst, QStringList({"file1.txt", "file10.txt", "file2.txt"}));

    // Switching back to a key the base class can map must still re-sort.
    m_pane->applySortKey(SortKey::Name, Qt::AscendingOrder);
    QStringList byName = visibleNames();
    byName.removeAll("..");
    byName.removeAll("folder1");
    QCOMPARE(byName, QStringList({"file1.txt", "file10.txt", "file2.txt"}));
}

void FilePaneSortTest::dialogNavigatesAndTogglesOrder()
{
    SortOptionsDialog dialog(SortKey::Name, Qt::AscendingOrder);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.selectedKey(), SortKey::Name);
    QCOMPARE(dialog.selectedOrder(), Qt::AscendingOrder);

    QTest::keyClick(&dialog, Qt::Key_Down);
    QCOMPARE(dialog.selectedKey(), SortKey::Size);
    QTest::keyClick(&dialog, Qt::Key_J);
    QCOMPARE(dialog.selectedKey(), SortKey::DateModified);
    QTest::keyClick(&dialog, Qt::Key_K);
    QCOMPARE(dialog.selectedKey(), SortKey::Size);

    // Space flips the direction the dialog will apply; it does not move.
    QTest::keyClick(&dialog, Qt::Key_Space);
    QCOMPARE(dialog.selectedOrder(), Qt::DescendingOrder);
    QCOMPARE(dialog.selectedKey(), SortKey::Size);
    QTest::keyClick(&dialog, Qt::Key_Space);
    QCOMPARE(dialog.selectedOrder(), Qt::AscendingOrder);

    // Escape leaves the pane's sort untouched.
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

void FilePaneSortTest::dialogDigitJumpsAndEnterAccepts()
{
    SortOptionsDialog dialog(SortKey::Name, Qt::AscendingOrder);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    // Row 5 in the listed order is Natural.
    QTest::keyClick(&dialog, Qt::Key_5);
    QCOMPARE(dialog.selectedKey(), SortKey::Natural);

    QTest::keyClick(&dialog, Qt::Key_Return);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dialog.selectedKey(), SortKey::Natural);
}

void FilePaneSortTest::savedSortIsRestoredOnANewPane()
{
    // Only the pane labelled LEFT persists the shared column layout, so the
    // saved sort has to come back on the next pane built from it.
    FilePane *saver = openPane("LEFT");
    QTRY_VERIFY_WITH_TIMEOUT(namesOf(saver).size() == 3, 5000);
    saver->applySortKey(SortKey::Size, Qt::DescendingOrder);
    QCOMPARE(namesOf(saver), QStringList({"file2.txt", "file10.txt", "file1.txt"}));
    // The sort is persisted once the layout-change window closes.
    QTest::qWait(50);
    delete saver;

    FilePane *restored = openPane("LEFT");
    QTRY_VERIFY_WITH_TIMEOUT(namesOf(restored).size() == 3, 5000);
    QCOMPARE(namesOf(restored), QStringList({"file2.txt", "file10.txt", "file1.txt"}));
    delete restored;

    QSettings settings;
    settings.remove("FilePane/ColumnsV2");
}

void FilePaneSortTest::headerClickSticksWhenAColumnLayoutIsSaved()
{
    // With a saved layout on disk the pane re-reads it on every layoutChanged,
    // which is exactly what a sort emits. The clicked column has to survive
    // that round trip instead of being reset to the stored one.
    FilePane *pane = openPane("LEFT");
    QTRY_VERIFY_WITH_TIMEOUT(namesOf(pane).size() == 3, 5000);
    pane->applySortKey(SortKey::Name, Qt::AscendingOrder);
    QCOMPARE(namesOf(pane), QStringList({"file1.txt", "file10.txt", "file2.txt"}));

    QTableView *view = fileViewOf(pane);
    QHeaderView *header = view->horizontalHeader();
    const int x = header->sectionViewportPosition(ColumnSize) + header->sectionSize(ColumnSize) / 2;
    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(x, header->height() / 2));
    QTest::qWait(50);

    QCOMPARE(header->sortIndicatorSection(), static_cast<int>(ColumnSize));
    QCOMPARE(namesOf(pane), QStringList({"file1.txt", "file10.txt", "file2.txt"}));

    delete pane;
    QSettings settings;
    settings.remove("FilePane/ColumnsV2");
}

void FilePaneSortTest::parentEntryShowsNoMetadata()
{
    QAbstractItemModel *model = fileView()->model();
    const QModelIndex root = fileView()->rootIndex();

    int parentRow = -1;
    for (int row = 0; row < model->rowCount(root); ++row) {
        if (model->index(row, ColumnName, root).data().toString() == "..") {
            parentRow = row;
            break;
        }
    }
    QVERIFY(parentRow >= 0);

    // Only the name is shown; the metadata would describe the folder above.
    for (const int column : {ColumnType, ColumnSize, ColumnCreated, ColumnModified,
                             ColumnMode, ColumnGit}) {
        const QString text = model->index(parentRow, column, root).data().toString();
        QVERIFY2(text.isEmpty(), qPrintable(QString("column %1 = %2").arg(column).arg(text)));
    }

    // A real folder in the same listing still shows its metadata.
    int folderRow = -1;
    for (int row = 0; row < model->rowCount(root); ++row) {
        if (model->index(row, ColumnName, root).data().toString() == "folder1") {
            folderRow = row;
            break;
        }
    }
    QVERIFY(folderRow >= 0);
    QVERIFY(!model->index(folderRow, ColumnModified, root).data().toString().isEmpty());
    QVERIFY(!model->index(folderRow, ColumnMode, root).data().toString().isEmpty());
}

QTEST_MAIN(FilePaneSortTest)
#include "FilePaneSortTest.moc"
