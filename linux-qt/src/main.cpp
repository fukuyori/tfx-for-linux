#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QIcon>

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    bool foreground = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-f") == 0 || std::strcmp(argv[i], "--foreground") == 0) {
            foreground = true;
            break;
        }
    }
    // Detach from the launching terminal so the shell prompt returns
    // immediately (as if started with `&`). Must happen before QApplication
    // exists: forking after Qt opens its display connection is unsafe.
    if (!foreground) {
        const pid_t pid = fork();
        if (pid > 0) {
            return 0;
        }
        if (pid == 0) {
            // New session: closing the terminal no longer delivers SIGHUP.
            setsid();
            // Detached runs must not print Qt runtime warnings over the shell
            // prompt they just returned; --foreground keeps them visible.
            const int devNull = ::open("/dev/null", O_RDWR);
            if (devNull >= 0) {
                ::dup2(devNull, STDIN_FILENO);
                ::dup2(devNull, STDOUT_FILENO);
                ::dup2(devNull, STDERR_FILENO);
                if (devNull > STDERR_FILENO) {
                    ::close(devNull);
                }
            }
        }
        // fork() failed: continue attached to the terminal.
    }

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
