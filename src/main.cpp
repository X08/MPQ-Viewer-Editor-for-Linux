#include <QApplication>

#include "mainwindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mpq-viewer"));
    QApplication::setOrganizationName(QStringLiteral("mpq-viewer"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setApplicationDisplayName(QStringLiteral("MPQ Viewer by X08"));
    QApplication::setDesktopFileName(QStringLiteral("mpq-viewer"));

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.size() > 1)
        w.openArchive(args.at(1));
    return app.exec();
}
