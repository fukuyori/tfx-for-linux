#include "FilePane.h"
#include "models/FileColumns.h"
#include "models/FileSystemProxyModel.h"

#include <QClipboard>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

// Integration tests for multi-selection driving the real FilePane: the
// selection built with Ctrl/Shift clicks is what move/copy/trash operate on,
// so it must survive the pane's deferred click handling.
class FilePaneSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void ctrlClickBuildsMultiSelection();
    void ctrlClickMultiSelectionSurvivesDeferredHandlers();
    void copySelectedPutsAllUrlsOnClipboard();
    void shiftClickRangeSelection();
    void multiSelectionSurvivesModelRefresh();
    void statusAndCopyAfterModelRefresh();
    void emptyAreaDragSelectsRange();
    void pressOnSelectedRowKeepsMultiSelectionForDrag();
    void pressAfterModelRefreshKeepsMultiSelection();
    void strictSelectedRowsRestoredAfterRefresh();

private:
    QTableView *fileView() const;
    int rowForName(const QString &name) const;
    void clickRow(int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    QTemporaryDir *m_dir = nullptr;
    FilePane *m_pane = nullptr;
};

void FilePaneSelectionTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void FilePaneSelectionTest::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());
    for (const QString &name : {"a.txt", "b.txt", "c.txt"}) {
        QFile file(m_dir->filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
    QVERIFY(QDir(m_dir->path()).mkdir("folder1"));
    QVERIFY(QDir(m_dir->path()).mkdir("folder2"));

    m_pane = new FilePane("TEST", m_dir->path());
    m_pane->resize(800, 600);
    m_pane->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_pane));

    // QFileSystemModel populates asynchronously; the list also shows a ".."
    // parent row.
    QTRY_VERIFY_WITH_TIMEOUT(
        fileView()->model()->rowCount(fileView()->rootIndex()) >= 5, 5000);
    QTest::qWait(50);
}

void FilePaneSelectionTest::cleanup()
{
    delete m_pane;
    m_pane = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

QTableView *FilePaneSelectionTest::fileView() const
{
    // Several table views share the "fileTable" object name; the file list is
    // the one visible in the pane's view stack.
    const QList<QTableView *> views = m_pane->findChildren<QTableView *>("fileTable");
    for (QTableView *view : views) {
        if (view->isVisible()) {
            return view;
        }
    }
    return nullptr;
}

int FilePaneSelectionTest::rowForName(const QString &name) const
{
    QAbstractItemModel *model = fileView()->model();
    const QModelIndex root = fileView()->rootIndex();
    for (int row = 0; row < model->rowCount(root); ++row) {
        if (model->index(row, ColumnName, root).data().toString() == name) {
            return row;
        }
    }
    return -1;
}

void FilePaneSelectionTest::clickRow(int row, Qt::KeyboardModifiers modifiers)
{
    QTableView *view = fileView();
    const QModelIndex index = view->model()->index(row, ColumnName, view->rootIndex());
    QVERIFY(index.isValid());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, modifiers,
                      view->visualRect(index).center());
    // Let the pane's deferred single-shot click/release handlers run.
    QTest::qWait(30);
}

void FilePaneSelectionTest::ctrlClickBuildsMultiSelection()
{
    clickRow(rowForName("a.txt"));
    QCOMPARE(m_pane->selectedUrls().size(), 1);

    clickRow(rowForName("c.txt"), Qt::ControlModifier);
    QCOMPARE(m_pane->selectedUrls().size(), 2);

    clickRow(rowForName("folder1"), Qt::ControlModifier);
    QCOMPARE(m_pane->selectedUrls().size(), 3);
}

void FilePaneSelectionTest::ctrlClickMultiSelectionSurvivesDeferredHandlers()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("b.txt"), Qt::ControlModifier);
    clickRow(rowForName("folder2"), Qt::ControlModifier);

    // Wait well past every deferred timer before checking what an operation
    // (copy/cut/trash) would actually see.
    QTest::qWait(200);
    QCOMPARE(m_pane->selectedUrls().size(), 3);
}

void FilePaneSelectionTest::copySelectedPutsAllUrlsOnClipboard()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("c.txt"), Qt::ControlModifier);
    clickRow(rowForName("folder1"), Qt::ControlModifier);
    QTest::qWait(100);

    m_pane->copySelected();
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    QVERIFY(mime);
    QCOMPARE(mime->urls().size(), 3);
}

void FilePaneSelectionTest::shiftClickRangeSelection()
{
    const int from = rowForName("a.txt");
    const int to = rowForName("c.txt");
    QVERIFY(from >= 0 && to >= 0);
    clickRow(from);
    clickRow(to, Qt::ShiftModifier);
    QTest::qWait(100);
    QCOMPARE(m_pane->selectedUrls().size(), qAbs(to - from) + 1);
}

// Selection ranges cover the proxy's synthesised columns (created/modified/
// mode/git), which do not survive a model layout change: after a sort or
// refresh the rows stay only partially selected, and operations that used
// QItemSelectionModel::selectedRows() (which requires every column selected)
// collapsed a multi-selection to the current row. This reproduces the
// "cannot move/copy/delete multiple items" report.
void FilePaneSelectionTest::multiSelectionSurvivesModelRefresh()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("b.txt"), Qt::ControlModifier);
    clickRow(rowForName("folder1"), Qt::ControlModifier);
    QCOMPARE(m_pane->selectedUrls().size(), 3);

    // Force the layout change that a sort, Git-status refresh, or
    // directory reload produces.
    auto *proxy = m_pane->findChild<FileSystemProxyModel *>();
    QVERIFY(proxy);
    proxy->invalidate();
    QTest::qWait(50);

    QCOMPARE(m_pane->selectedUrls().size(), 3);
}

void FilePaneSelectionTest::statusAndCopyAfterModelRefresh()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("c.txt"), Qt::ControlModifier);

    auto *proxy = m_pane->findChild<FileSystemProxyModel *>();
    QVERIFY(proxy);
    proxy->invalidate();
    QTest::qWait(50);

    m_pane->copySelected();
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    QVERIFY(mime);
    QCOMPARE(mime->urls().size(), 2);
}

// Rubber-band selection: press on the empty area below the rows and drag up
// across them; every row crossed must end up selected.
void FilePaneSelectionTest::emptyAreaDragSelectsRange()
{
    QTableView *view = fileView();
    const QModelIndex root = view->rootIndex();
    const int rowCount = view->model()->rowCount(root);
    const QModelIndex lastIndex = view->model()->index(rowCount - 1, ColumnName, root);
    const QRect lastRect = view->visualRect(lastIndex);

    // Start well below the last row (empty viewport area), drag up to the
    // second row so the ".." row stays out of the band.
    const QPoint start(lastRect.center().x(), lastRect.bottom() + 60);
    const QModelIndex secondIndex = view->model()->index(1, ColumnName, root);
    const QPoint end(lastRect.center().x(), view->visualRect(secondIndex).center().y());

    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    const QPoint mid((start.x() + end.x()) / 2, (start.y() + end.y()) / 2);
    for (const QPoint &pos : {mid, end}) {
        QMouseEvent move(QEvent::MouseMove, pos, view->viewport()->mapToGlobal(pos),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QTest::qWait(10);
    }
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QTest::qWait(50);

    QCOMPARE(m_pane->selectedUrls().size(), rowCount - 1);
}

// A plain press on one item of a multi-selection must keep the selection
// until release, so the whole selection can be dragged; the drag payload has
// to carry every selected file. Only a completed click (release without a
// drag) collapses to the pressed row.
void FilePaneSelectionTest::pressOnSelectedRowKeepsMultiSelectionForDrag()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("b.txt"), Qt::ControlModifier);
    clickRow(rowForName("c.txt"), Qt::ControlModifier);
    QCOMPARE(m_pane->selectedUrls().size(), 3);

    QTableView *view = fileView();
    const QModelIndex index =
        view->model()->index(rowForName("b.txt"), ColumnName, view->rootIndex());
    const QPoint pos = view->visualRect(index).center();
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    QTest::qWait(20);
    QCOMPARE(m_pane->selectedUrls().size(), 3);

    // Wiggle below the drag threshold: without ItemIsDragEnabled flags the
    // base view rubber-band-selected here and collapsed the selection before
    // a drag could begin.
    const QPoint wiggle = pos + QPoint(2, 2);
    QMouseEvent move(QEvent::MouseMove, wiggle, view->viewport()->mapToGlobal(wiggle),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QTest::qWait(10);

    // While pressed (the moment a drag would start), all three items remain
    // selected and would ship in the drag's mime data.
    QCOMPARE(m_pane->selectedUrls().size(), 3);
    QMimeData *mime = view->model()->mimeData(view->selectionModel()->selectedIndexes());
    QVERIFY(mime);
    QCOMPARE(mime->urls().size(), 3);
    delete mime;

    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, wiggle);
    QTest::qWait(50);
    // A completed click collapses to the clicked row (standard behaviour).
    QCOMPARE(m_pane->selectedUrls().size(), 1);
}

void FilePaneSelectionTest::pressAfterModelRefreshKeepsMultiSelection()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("b.txt"), Qt::ControlModifier);
    clickRow(rowForName("c.txt"), Qt::ControlModifier);

    auto *proxy = m_pane->findChild<FileSystemProxyModel *>();
    QVERIFY(proxy);
    proxy->invalidate();
    QTest::qWait(50);

    // Press on a synthesised column's cell (the ones a layout change drops
    // from the stored selection); the multi-selection must survive the press.
    QTableView *view = fileView();
    const QModelIndex nameIndex =
        view->model()->index(rowForName("b.txt"), ColumnName, view->rootIndex());
    QModelIndex pressIndex = nameIndex.sibling(nameIndex.row(), ColumnMode);
    if (view->visualRect(pressIndex).isEmpty()) {
        pressIndex = nameIndex;
    }
    const QPoint pos = view->visualRect(pressIndex).center();
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    QTest::qWait(20);
    QCOMPARE(m_pane->selectedUrls().size(), 3);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    QTest::qWait(50);
}

void FilePaneSelectionTest::strictSelectedRowsRestoredAfterRefresh()
{
    clickRow(rowForName("a.txt"));
    clickRow(rowForName("c.txt"), Qt::ControlModifier);

    auto *proxy = m_pane->findChild<FileSystemProxyModel *>();
    QVERIFY(proxy);
    proxy->invalidate();
    QTest::qWait(50);

    // Selection normalization re-selects full rows, so even Qt's strict
    // all-columns selectedRows() sees the whole multi-selection again.
    QCOMPARE(fileView()->selectionModel()->selectedRows().size(), 2);
}

QTEST_MAIN(FilePaneSelectionTest)
#include "FilePaneSelectionTest.moc"
