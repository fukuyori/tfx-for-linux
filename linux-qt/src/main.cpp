#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("tfx");
    QApplication::setOrganizationName("fukuyori");
    // Matches tfx.desktop so Wayland compositors associate the window with
    // the desktop entry and its icon.
    QGuiApplication::setDesktopFileName("tfx");

    QIcon appIcon;
    for (const int size : {32, 48, 64, 128, 256}) {
        appIcon.addFile(QStringLiteral(":/icons/tfx-%1.png").arg(size), QSize(size, size));
    }
    QApplication::setWindowIcon(appIcon);

    QString initialPath = QDir::currentPath();
    QString geometryOverride;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == "-g" || arg == "--geometry") {
            if (i + 1 < args.size()) {
                geometryOverride = args.at(++i);
            }
            continue;
        }
        if (arg.startsWith("--geometry=")) {
            geometryOverride = arg.mid(QString("--geometry=").size());
            continue;
        }
        if (!arg.startsWith('-')) {
            QFileInfo info(arg);
            if (info.exists() && info.isDir()) {
                initialPath = info.absoluteFilePath();
                break;
            }
        }
    }

    MainWindow window(initialPath, geometryOverride);
    window.show();

    return app.exec();
}
