#include "AppConfig.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool writeConfig(const QString &configRoot, const QString &content)
{
    const QString directory = QDir(configRoot).filePath("tfx");
    if (!QDir().mkpath(directory)) {
        return false;
    }
    QFile file(QDir(directory).filePath("config.toml"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << content;
    return true;
}
}

class AppConfigTest : public QObject
{
    Q_OBJECT

private slots:
    void lightThemeAppliesPaletteAndColorOverrides();
    void invalidThemeFallsBackToDarkAndWarns();
    void openWithMappingsLoadExtensionsAndFallback();
    void malformedConfigFallsBackToDefaults();
};

void AppConfigTest::lightThemeAppliesPaletteAndColorOverrides()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    qputenv("XDG_CONFIG_HOME", temp.path().toUtf8());
    QVERIFY(writeConfig(temp.path(),
                        "version = 1\n"
                        "theme = \"light\"\n"
                        "\n"
                        "[colors]\n"
                        "fileForeground = \"#112233\"\n"));

    const AppConfig config = AppConfig::loadOrCreate();

    QCOMPARE(config.theme.name, QString("light"));
    QCOMPARE(config.colors.panelBackground, QString("#F8FAFC"));
    QCOMPARE(config.colors.foreground, QString("#112233"));
    QCOMPARE(config.colors.directoryForeground, QString("#123D66"));
    QCOMPARE(config.terminalColorScheme, QString("BlackOnLightYellow"));
    QVERIFY(config.warningText().isEmpty());
}

void AppConfigTest::invalidThemeFallsBackToDarkAndWarns()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    qputenv("XDG_CONFIG_HOME", temp.path().toUtf8());
    QVERIFY(writeConfig(temp.path(),
                        "version = 1\n"
                        "theme = \"neon\"\n"));

    const AppConfig config = AppConfig::loadOrCreate();

    QCOMPARE(config.theme.name, QString("dark"));
    QCOMPARE(config.colors.panelBackground, QString("#151A1E"));
    QVERIFY(config.warningText().contains("Invalid theme value"));
}

void AppConfigTest::openWithMappingsLoadExtensionsAndFallback()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    qputenv("XDG_CONFIG_HOME", temp.path().toUtf8());
    QVERIFY(writeConfig(temp.path(),
                        "version = 1\n"
                        "\n"
                        "[openWith]\n"
                        "md = \"code\"\n"
                        "* = \"xdg-open\"\n"));

    const AppConfig config = AppConfig::loadOrCreate();

    QCOMPARE(config.openWith.value("md"), QString("code"));
    QCOMPARE(config.openWith.value("*"), QString("xdg-open"));
    QVERIFY(config.warningText().isEmpty());
}

void AppConfigTest::malformedConfigFallsBackToDefaults()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    qputenv("XDG_CONFIG_HOME", temp.path().toUtf8());

    // Invalid UTF-8, an unterminated string, an oversized value, control
    // characters, and junk syntax must not crash the loader; parsing degrades
    // to defaults (possibly with warnings).
    QByteArray content;
    content += "version = 1\n";
    content += "theme = \"dark\n";              // unterminated string
    content += "\xFF\xFE\x80 = \"broken\"\n";   // invalid UTF-8 key
    content += "[colors]\n";
    content += "fileForeground = \"#";
    content += QByteArray(2 * 1024 * 1024, 'F'); // oversized value
    content += "\"\n";
    content += "[[commands]]\n";
    content += "name = \"a\x01\x02\"\n";        // control characters
    content += "= = =\n";                        // junk line
    content += "[]\n";                           // empty section

    const QString directory = QDir(temp.path()).filePath("tfx");
    QVERIFY(QDir().mkpath(directory));
    QFile file(QDir(directory).filePath("config.toml"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(content) == content.size());
    file.close();

    const AppConfig config = AppConfig::loadOrCreate();

    // Defaults survive: dark palette and no crash.
    QCOMPARE(config.theme.name, QString("dark"));
    QCOMPARE(config.colors.panelBackground, QString("#151A1E"));
}

QTEST_GUILESS_MAIN(AppConfigTest)

#include "AppConfigTest.moc"
