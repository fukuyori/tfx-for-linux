#include "MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("tfx");
    QApplication::setOrganizationName("fukuyori");

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
