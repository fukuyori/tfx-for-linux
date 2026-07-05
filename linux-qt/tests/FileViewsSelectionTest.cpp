#include "views/FileViews.h"

#include <QStandardItemModel>
#include <QtTest>

#include <algorithm>

// Regression tests for multi-selection in the details view: Ctrl+click must
// add/remove rows and the resulting selection must survive so that
// move/copy/trash can operate on all selected items.
class FileViewsSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void plainClickSelectsSingleRow();
    void ctrlClickAddsRowToSelection();
    void ctrlClickRemovesRowFromSelection();
    void shiftClickSelectsRange();
    void plainClickCollapsesMultiSelection();

private:
    void clickRow(int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    QList<int> selectedRows() const;

    QStandardItemModel *m_model = nullptr;
    FileTableView *m_view = nullptr;
};

void FileViewsSelectionTest::init()
{
    m_model = new QStandardItemModel(6, kColumnCount, this);
    for (int row = 0; row < m_model->rowCount(); ++row) {
        for (int column = 0; column < kColumnCount; ++column) {
            m_model->setItem(row, column,
                             new QStandardItem(QString("r%1c%2").arg(row).arg(column)));
        }
    }

    m_view = new FileTableView;
    m_view->setModel(m_model);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragDrop);
    m_view->resize(640, 480);
    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));
}

void FileViewsSelectionTest::cleanup()
{
    delete m_view;
    m_view = nullptr;
    delete m_model;
    m_model = nullptr;
}

void FileViewsSelectionTest::clickRow(int row, Qt::KeyboardModifiers modifiers)
{
    const QModelIndex index = m_model->index(row, 0);
    QVERIFY(index.isValid());
    const QPoint pos = m_view->visualRect(index).center();
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, modifiers, pos);
    // The view finalizes plain clicks through a deferred single-shot timer.
    QTest::qWait(20);
}

QList<int> FileViewsSelectionTest::selectedRows() const
{
    QList<int> rows;
    const QModelIndexList indexes = m_view->selectionModel()->selectedRows();
    rows.reserve(indexes.size());
    for (const QModelIndex &index : indexes) {
        rows << index.row();
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void FileViewsSelectionTest::plainClickSelectsSingleRow()
{
    clickRow(1);
    QCOMPARE(selectedRows(), (QList<int>{1}));
}

void FileViewsSelectionTest::ctrlClickAddsRowToSelection()
{
    clickRow(0);
    clickRow(2, Qt::ControlModifier);
    QCOMPARE(selectedRows(), (QList<int>{0, 2}));

    clickRow(4, Qt::ControlModifier);
    QCOMPARE(selectedRows(), (QList<int>{0, 2, 4}));
}

void FileViewsSelectionTest::ctrlClickRemovesRowFromSelection()
{
    clickRow(0);
    clickRow(2, Qt::ControlModifier);
    clickRow(4, Qt::ControlModifier);
    QCOMPARE(selectedRows(), (QList<int>{0, 2, 4}));

    clickRow(2, Qt::ControlModifier);
    QCOMPARE(selectedRows(), (QList<int>{0, 4}));
}

void FileViewsSelectionTest::shiftClickSelectsRange()
{
    clickRow(1);
    clickRow(3, Qt::ShiftModifier);
    QCOMPARE(selectedRows(), (QList<int>{1, 2, 3}));
}

void FileViewsSelectionTest::plainClickCollapsesMultiSelection()
{
    clickRow(0);
    clickRow(2, Qt::ControlModifier);
    QCOMPARE(selectedRows(), (QList<int>{0, 2}));

    clickRow(1);
    QCOMPARE(selectedRows(), (QList<int>{1}));
}

QTEST_MAIN(FileViewsSelectionTest)
#include "FileViewsSelectionTest.moc"
