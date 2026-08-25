#include "FilePane.h"

#include <QApplication>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// The pane title bar carries the active/inactive [colors] titleBarBackground*
// values. Its header row holds an expanding path container, and the generic
// "QWidget { background: ... }" rule in the theme also matches that container:
// Qt then marks it styled and paints it over the title bar, hiding the colour
// the user configured. These tests pin the header down to the configured
// colour across its whole width, not just the layout margins.
class PaneTitleBarTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void pathContainerKeepsItsStyleSheetHook();
    void titleBarPaintsTheConfiguredColourAcrossItsWidth();

private:
    QWidget *titleBar() const;
    QColor colourAt(QWidget *widget, int x) const;

    QTemporaryDir *m_dir = nullptr;
    FilePane *m_pane = nullptr;
};

// A translucent panel background (a plain [opacity] background setting) makes
// an accidental overpaint obvious: it blends rather than matching exactly.
static const char *kThemeStyleSheet =
    "QMainWindow, QWidget { background: rgba(18,8,0,0.600); color: #FFE7B0; }"
    "\nQWidget#paneTitleBar { background: #1C0D00; }"
    "\nQWidget#paneTitleBar[activePane=\"true\"] { background: #4A2A08; }"
    "\nQStackedWidget#panePathStack { background: transparent; }\n";

void PaneTitleBarTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void PaneTitleBarTest::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());
    qApp->setStyleSheet(QString::fromUtf8(kThemeStyleSheet));

    m_pane = new FilePane("TEST", m_dir->path());
    m_pane->resize(900, 600);
    m_pane->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_pane));
    QTest::qWait(50);
}

void PaneTitleBarTest::cleanup()
{
    delete m_pane;
    m_pane = nullptr;
    delete m_dir;
    m_dir = nullptr;
    qApp->setStyleSheet(QString());
}

QWidget *PaneTitleBarTest::titleBar() const
{
    return m_pane->findChild<QWidget *>("paneTitleBar");
}

QColor PaneTitleBarTest::colourAt(QWidget *widget, int x) const
{
    const QImage image = widget->grab().toImage();
    return QColor(image.pixel(x, image.height() / 2));
}

void PaneTitleBarTest::pathContainerKeepsItsStyleSheetHook()
{
    // The theme clears this container by object name; renaming it silently
    // brings the overpaint back.
    QVERIFY(titleBar());
    QVERIFY(titleBar()->findChild<QStackedWidget *>("panePathStack"));
}

void PaneTitleBarTest::titleBarPaintsTheConfiguredColourAcrossItsWidth()
{
    QWidget *bar = titleBar();
    QVERIFY(bar);
    QVERIFY(bar->isVisible());

    m_pane->setActive(false);
    QTest::qWait(20);
    QCOMPARE(colourAt(bar, 2).name().toUpper(), QStringLiteral("#1C0D00"));
    QCOMPARE(colourAt(bar, bar->width() / 2).name().toUpper(), QStringLiteral("#1C0D00"));

    m_pane->setActive(true);
    QTest::qWait(20);
    QCOMPARE(colourAt(bar, 2).name().toUpper(), QStringLiteral("#4A2A08"));
    // The centre sits over the path container: the regression showed up here
    // as the panel background blended over the title bar colour.
    QCOMPARE(colourAt(bar, bar->width() / 2).name().toUpper(), QStringLiteral("#4A2A08"));
}

QTEST_MAIN(PaneTitleBarTest)
#include "PaneTitleBarTest.moc"
