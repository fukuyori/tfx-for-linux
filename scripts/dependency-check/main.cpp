#include <QApplication>
#include <QDBusConnection>
#include <qtermwidget.h>

// This executable is only built, never run.
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTermWidget terminal(0);
    return QDBusConnection::sessionBus().isConnected() ? 0 : 1;
}
