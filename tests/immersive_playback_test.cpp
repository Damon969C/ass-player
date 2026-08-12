#include "main_window.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        QTextStream(stderr) << message << '\n';
        ++failures;
    }
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SubtitleSidecarPlayerTests"));
    QApplication::setApplicationName(QStringLiteral("ImmersivePlaybackTest"));

    QTemporaryDir settingsDirectory;
    expect(settingsDirectory.isValid(), "temporary settings directory should be available");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    MainWindow window;
    window.resize(1100, 700);
    window.showNormal();
    window.show();
    app.processEvents();

    auto *fullscreenButton = window.findChild<QPushButton *>(QStringLiteral("fullscreenButton"));
    QWidget *toolbarStrip = window.findChild<QWidget *>(QStringLiteral("toolbarStrip"));
    QWidget *subtitlePane = window.findChild<QWidget *>(QStringLiteral("subtitlePane"));
    QWidget *controlStrip = window.findChild<QWidget *>(QStringLiteral("controlStrip"));
    QWidget *videoHost = window.findChild<QWidget *>(QStringLiteral("videoHost"));
    QWidget *playerPane = window.findChild<QWidget *>(QStringLiteral("playerPane"));
    QTimer *autoHideTimer = window.findChild<QTimer *>(QStringLiteral("immersiveControlsTimer"));

    expect(fullscreenButton, "fullscreen button should exist");
    expect(toolbarStrip && subtitlePane && controlStrip && videoHost && playerPane, "immersive widgets should exist");
    expect(autoHideTimer, "immersive auto-hide timer should exist");
    if (!fullscreenButton || !toolbarStrip || !subtitlePane || !controlStrip || !videoHost || !playerPane || !autoHideTimer) {
        return 1;
    }

    expect(!fullscreenButton->isEnabled(), "fullscreen button should stay disabled until media is loaded");
    expect(autoHideTimer->interval() == 2000, "immersive controls should use a two-second inactivity timeout");

    fullscreenButton->setEnabled(true);
    fullscreenButton->click();
    app.processEvents();

    expect(toolbarStrip->isHidden(), "toolbar should hide in immersive mode");
    expect(subtitlePane->isHidden(), "subtitle pane should hide in immersive mode");
    expect(controlStrip->isVisible(), "immersive controls should initially be visible");
    expect(controlStrip->property("immersive").toBool(), "control strip should use immersive styling");
    expect(playerPane->layout()->indexOf(controlStrip) < 0, "immersive controls should float outside the player layout");
    expect(autoHideTimer->isActive(), "immersive controls auto-hide timer should start on entry");

    const bool timeoutInvoked = QMetaObject::invokeMethod(autoHideTimer, "timeout", Qt::DirectConnection);
    app.processEvents();
    expect(timeoutInvoked, "auto-hide timeout should be invokable by the Qt meta-object system");
    expect(controlStrip->isHidden(), "immersive controls should hide after inactivity");

    QMouseEvent mouseMove(
        QEvent::MouseMove,
        QPointF(10.0, 10.0),
        QPointF(10.0, 10.0),
        Qt::NoButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(videoHost, &mouseMove);
    app.processEvents();
    expect(controlStrip->isVisible(), "mouse activity should reveal immersive controls");
    expect(autoHideTimer->isActive(), "mouse activity should restart the inactivity timeout");

    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
    app.processEvents();
    expect(toolbarStrip->isVisible(), "Escape should restore the toolbar");
    expect(subtitlePane->isVisible(), "Escape should restore the subtitle pane");
    expect(controlStrip->isVisible(), "Escape should restore the regular control strip");
    expect(!controlStrip->property("immersive").toBool(), "Escape should clear immersive styling");
    expect(playerPane->layout()->indexOf(controlStrip) >= 0, "Escape should return controls to the player layout");
    expect(!autoHideTimer->isActive(), "Escape should stop the immersive inactivity timer");

    window.showMaximized();
    app.processEvents();
    expect(window.isMaximized(), "test window should reach the maximized state");
    fullscreenButton->click();
    app.processEvents();
    expect(window.isFullScreen(), "immersive playback should use system fullscreen from a maximized window");
    fullscreenButton->click();
    app.processEvents();
    expect(window.isMaximized() && !window.isFullScreen(), "leaving system fullscreen should restore the maximized window");

    window.close();
    return failures == 0 ? 0 : 1;
}
