#include "MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("tfx");
    QApplication::setOrganizationName("fukuyori");

    QString initialPath = QDir::currentPath();
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith('-')) {
            QFileInfo info(args.at(i));
            if (info.exists() && info.isDir()) {
                initialPath = info.absoluteFilePath();
                break;
            }
        }
    }

    MainWindow window(initialPath);
    window.show();

    return app.exec();
}
