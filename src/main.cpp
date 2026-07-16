#include "main_window.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("SubtitleSidecarPlayer");
    QApplication::setApplicationName("Subtitle Sidecar Player Qt");
    const QIcon appIcon(QStringLiteral(":/icons/app.ico"));
    app.setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.resize(1280, 760);
    window.setMinimumSize(960, 560);
    window.show();

    return app.exec();
}
