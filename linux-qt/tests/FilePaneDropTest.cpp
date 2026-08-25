#include "FilePane.h"
#include "models/FileColumns.h"
#include "views/FileViews.h"

#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

// Drop targeting: the highlight drawn during a drag and the directory the drop
// actually writes into come from the same pair of functions, so the feedback
// cannot promise a destination the drop will not use.
class FilePaneDropTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void onlyDirectoryRowsAcceptADrop();
    void droppingOnAFileTargetsTheListedFolder();
    void droppingOnAFolderTargetsThatFolder();
    void droppingOnTheParentRowTargetsTheParentDirectory();
    void dropActionFollowsTheControlModifier();
    void hintNamesTheDestinationAndTheAction();
    void dropHighlightCoversOnlyTheName();

private:
    QTableView *fileView() const;
    QModelIndex rowFor(const QString &name) const;

    QTemporaryDir *m_dir = nullptr;
    FilePane *m_pane = nullptr;
};

void FilePaneDropTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void FilePaneDropTest::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());
    QFile file(m_dir->filePath("note.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    QVERIFY(QDir(m_dir->path()).mkdir("target"));

    m_pane = new FilePane("TEST", m_dir->path());
    m_pane->resize(800, 600);
    m_pane->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_pane));
    QTRY_VERIFY_WITH_TIMEOUT(
        fileView()->model()->rowCount(fileView()->rootIndex()) >= 3, 5000);
    QTest::qWait(50);
}

void FilePaneDropTest::cleanup()
{
    delete m_pane;
    m_pane = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

QTableView *FilePaneDropTest::fileView() const
{
    const QList<QTableView *> views = m_pane->findChildren<QTableView *>("fileTable");
    for (QTableView *view : views) {
        if (view->isVisible()) {
            return view;
        }
    }
    return nullptr;
}

QModelIndex FilePaneDropTest::rowFor(const QString &name) const
{
    QAbstractItemModel *model = fileView()->model();
    const QModelIndex root = fileView()->rootIndex();
    for (int row = 0; row < model->rowCount(root); ++row) {
        const QModelIndex index = model->index(row, ColumnName, root);
        if (index.data().toString() == name) {
            return index;
        }
    }
    return QModelIndex();
}

void FilePaneDropTest::onlyDirectoryRowsAcceptADrop()
{
    QVERIFY(m_pane->acceptsDropOnRow(rowFor("target")));
    // A file cannot receive a drop, so its row must not be framed as if it can.
    QVERIFY(!m_pane->acceptsDropOnRow(rowFor("note.txt")));
    QVERIFY(!m_pane->acceptsDropOnRow(QModelIndex()));
}

void FilePaneDropTest::droppingOnAFileTargetsTheListedFolder()
{
    const QString listed = QDir(m_dir->path()).canonicalPath();
    QCOMPARE(m_pane->dropDestinationDirectory(rowFor("note.txt")), listed);
    // Nothing under the cursor lands in the listed folder too.
    QCOMPARE(m_pane->dropDestinationDirectory(QModelIndex()), listed);
}

void FilePaneDropTest::droppingOnAFolderTargetsThatFolder()
{
    QCOMPARE(m_pane->dropDestinationDirectory(rowFor("target")),
             QDir(m_dir->filePath("target")).canonicalPath());
}

void FilePaneDropTest::droppingOnTheParentRowTargetsTheParentDirectory()
{
    const QModelIndex parentRow = rowFor("..");
    QVERIFY(parentRow.isValid());
    QVERIFY(m_pane->acceptsDropOnRow(parentRow));
    // Resolved, so neither the drop nor the label carries a trailing "/..".
    QCOMPARE(m_pane->dropDestinationDirectory(parentRow),
             QFileInfo(QDir(m_dir->path()).canonicalPath()).absolutePath());
}

void FilePaneDropTest::dropActionFollowsTheControlModifier()
{
    QCOMPARE(dropActionForModifiers(Qt::NoModifier), Qt::MoveAction);
    QCOMPARE(dropActionForModifiers(Qt::ShiftModifier), Qt::MoveAction);
    QCOMPARE(dropActionForModifiers(Qt::ControlModifier), Qt::CopyAction);
    QCOMPARE(dropActionForModifiers(Qt::ControlModifier | Qt::ShiftModifier), Qt::CopyAction);
}

void FilePaneDropTest::hintNamesTheDestinationAndTheAction()
{
    const QString moveHint = dropHintText("target", Qt::MoveAction);
    const QString copyHint = dropHintText("target", Qt::CopyAction);
    QVERIFY(moveHint.contains("target"));
    QVERIFY(copyHint.contains("target"));
    QVERIFY(moveHint != copyHint);
    // No destination, no badge.
    QVERIFY(dropHintText(QString(), Qt::MoveAction).isEmpty());

    // The label names the resolved folder, not the row under the cursor.
    QCOMPARE(FilePane::displayNameForDirectory(m_dir->filePath("target")), QStringLiteral("target"));
    QCOMPARE(FilePane::displayNameForDirectory("/"), QStringLiteral("/"));
}

// Exposes the protected measurement the drop highlight is drawn from.
class NameExtentProbe : public FileTableView
{
public:
    using FileTableView::FileTableView;
    QRect measure(const QModelIndex &index) const { return nameExtent(index); }
    QRect cellOf(const QModelIndex &index) const { return visualRect(index); }
};

void FilePaneDropTest::dropHighlightCoversOnlyTheName()
{
    QStandardItemModel model(2, kColumnCount);
    model.setItem(0, ColumnName, new QStandardItem("target"));
    model.setItem(1, ColumnName, new QStandardItem("a-much-longer-folder-name"));

    NameExtentProbe view;
    view.setModel(&model);
    view.resize(900, 200);
    for (int column = 0; column < kColumnCount; ++column) {
        view.setColumnWidth(column, 300);
    }
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    const QModelIndex shortName = model.index(0, ColumnName);
    const QModelIndex longName = model.index(1, ColumnName);
    const QRect shortExtent = view.measure(shortName);
    const QRect longExtent = view.measure(longName);

    // The highlight follows the text, so it must be far narrower than both the
    // name column and the row, which is what used to be framed.
    QVERIFY(shortExtent.width() > 0);
    QVERIFY(shortExtent.width() < view.cellOf(shortName).width());
    QVERIFY(shortExtent.width() < view.viewport()->width() / 2);
    // A longer name takes a correspondingly wider frame.
    QVERIFY(longExtent.width() > shortExtent.width());
    // Still bounded by the column it lives in.
    QVERIFY(longExtent.width() <= view.cellOf(longName).width());
}

QTEST_MAIN(FilePaneDropTest)
#include "FilePaneDropTest.moc"
