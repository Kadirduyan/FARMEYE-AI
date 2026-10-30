#include "ui/mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QSurfaceFormat>
#include <opencv2/core.hpp>

int main(int argc, char *argv[])
{
    // DNN tüm çekirdekleri yakmasın — sistem akıcı kalsın
    cv::setNumThreads(2);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setVersion(2, 1);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FarmEye AI"));
    app.setOrganizationName(QStringLiteral("FarmEye"));

    // SQLite eklentisi: build-ui/sqldrivers/qsqlite.dll
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/sqldrivers"));
    QCoreApplication::addLibraryPath(QDir(QCoreApplication::applicationDirPath())
                                         .absoluteFilePath(QStringLiteral("plugins")));

    farmeye::MainWindow window;
    window.showMaximized();
    window.raise();
    window.activateWindow();
    return app.exec();
}
