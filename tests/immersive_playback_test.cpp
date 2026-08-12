#include "main_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegion>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <functional>

namespace {
int failures = 0;

QWidget *topLevelWidget(const QString &objectName)
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == objectName) {
            return widget;
        }
    }
    return nullptr;
}

class WindowStateRecorder final : public QObject {
public:
    QVector<Qt::WindowStates> states;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::WindowStateChange) {
            if (const auto *widget = qobject_cast<const QWidget *>(watched)) {
                states.append(widget->windowState());
            }
        }
        return false;
    }
};

void expect(bool condition, const char *message)
{
    if (!condition) {
        QTextStream(stderr) << message << '\n';
        ++failures;
    }
}

bool waitFor(const std::function<bool()> &condition, int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    return condition();
}

void processEventsFor(int durationMs)
{
    QThread::msleep(durationMs);
#ifdef Q_OS_WIN
    MSG message;
    int processedMessages = 0;
    while (processedMessages < 512 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        ++processedMessages;
    }
    QCoreApplication::sendPostedEvents();
#else
    QApplication::processEvents(QEventLoop::AllEvents);
#endif
}

}

int main(int argc, char *argv[])
{
#ifndef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SubtitleSidecarPlayerTests"));
    QApplication::setApplicationName(QStringLiteral("ImmersivePlaybackTest"));

    QTemporaryDir settingsDirectory;
    expect(settingsDirectory.isValid(), "temporary settings directory should be available");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    MainWindow window;
    WindowStateRecorder windowStateRecorder;
    window.installEventFilter(&windowStateRecorder);
    window.resize(1100, 700);
    window.showNormal();
    window.show();
    app.processEvents();
    const QSize normalWindowSize = window.size();

    auto *fullscreenButton = window.findChild<QPushButton *>(QStringLiteral("fullscreenButton"));
    auto *playPauseButton = window.findChild<QPushButton *>(QStringLiteral("playPauseButton"));
    auto *repeatToggle = window.findChild<QCheckBox *>(QStringLiteral("repeatToggle"));
    QWidget *toolbarStrip = window.findChild<QWidget *>(QStringLiteral("toolbarStrip"));
    QWidget *subtitlePane = window.findChild<QWidget *>(QStringLiteral("subtitlePane"));
    QWidget *controlStrip = window.findChild<QWidget *>(QStringLiteral("controlStrip"));
    QWidget *videoHost = window.findChild<QWidget *>(QStringLiteral("videoHost"));
    QWidget *playerPane = window.findChild<QWidget *>(QStringLiteral("playerPane"));
    QTimer *autoHideTimer = window.findChild<QTimer *>(QStringLiteral("immersiveControlsTimer"));
    QToolButton *maximizeButton = nullptr;
    for (QToolButton *button : window.findChildren<QToolButton *>()) {
        if (button->text() != QStringLiteral("−") && button->text() != QStringLiteral("×")) {
            maximizeButton = button;
            break;
        }
    }

    expect(fullscreenButton, "fullscreen button should exist");
    expect(playPauseButton && repeatToggle, "playback and repeat controls should exist");
    expect(toolbarStrip && subtitlePane && controlStrip && videoHost && playerPane, "immersive widgets should exist");
    expect(autoHideTimer, "immersive auto-hide timer should exist");
    expect(maximizeButton, "maximize/restore button should exist");
    if (!fullscreenButton || !playPauseButton || !repeatToggle || !toolbarStrip || !subtitlePane || !controlStrip || !videoHost || !playerPane || !autoHideTimer || !maximizeButton) {
        return 1;
    }

    expect(repeatToggle->text() == QStringLiteral("重复"), "repeat switch should use the requested label");
    expect(!repeatToggle->isChecked(), "repeat mode should default to off");
    expect(toolbarStrip->layout()->indexOf(repeatToggle) == toolbarStrip->layout()->indexOf(playPauseButton) + 1,
        "repeat switch should immediately follow play/pause");
    repeatToggle->click();
    expect(repeatToggle->isChecked(), "repeat switch should be interactive");
    repeatToggle->click();

    expect(!fullscreenButton->isEnabled(), "fullscreen button should stay disabled until media is loaded");
    expect(autoHideTimer->interval() == 2000, "immersive controls should use a two-second inactivity timeout");
    const WId videoHostWindowId = videoHost->winId();
    const QRegion regularVideoMask = videoHost->mask();
    expect(!regularVideoMask.isEmpty(), "regular video host should apply a real rounded clipping mask");
    expect(!regularVideoMask.contains(QPoint(0, 0)), "rounded video mask should exclude the square corner");
    expect(regularVideoMask.contains(videoHost->rect().center()), "rounded video mask should retain the video center");

    fullscreenButton->setEnabled(true);
    fullscreenButton->click();
    app.processEvents();

    expect(toolbarStrip->isHidden(), "toolbar should hide in immersive mode");
    expect(subtitlePane->isHidden(), "subtitle pane should hide in immersive mode");
    expect(controlStrip->isVisible(), "immersive controls should initially be visible");
    expect(controlStrip->property("immersive").toBool(), "control strip should use immersive styling");
    expect(playerPane->layout()->indexOf(controlStrip) < 0, "immersive controls should float outside the player layout");
    expect(autoHideTimer->isActive(), "immersive controls auto-hide timer should start on entry");
    expect(videoHost->mask().isEmpty(), "immersive video should clear rounded clipping and fill the screen");

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
    expect(!videoHost->mask().isEmpty(), "leaving immersive mode should restore rounded video clipping");

    bool hasDistinctMaximizedWorkArea = true;
#ifdef Q_OS_WIN
    // A headless Wine desktop has no taskbar. Qt therefore classifies any
    // frameless maximized window (monitor-sized) as full-screen; skip only the
    // main-window state assertions that require a distinct Windows work area.
    hasDistinctMaximizedWorkArea = window.screen()->availableGeometry() != window.screen()->geometry();
#endif
    maximizeButton->click();
    expect(waitFor([&window]() { return window.isMaximized(); }), "test window should reach the maximized state");
    windowStateRecorder.states.clear();
    fullscreenButton->click();
    QWidget *immersiveFullscreenWindow = nullptr;
    expect(waitFor([&immersiveFullscreenWindow]() {
        immersiveFullscreenWindow = topLevelWidget(QStringLiteral("immersiveFullscreenWindow"));
        return immersiveFullscreenWindow && immersiveFullscreenWindow->isFullScreen();
    }), "immersive playback should open a dedicated system fullscreen window from a maximized main window");
    if (hasDistinctMaximizedWorkArea) {
        expect(window.isMaximized() && !window.isFullScreen(),
            "system fullscreen playback should leave the main window maximized");
    }
    expect(!windowStateRecorder.states.contains(Qt::WindowNoState),
        "entering system fullscreen should not change the main window to normal state");
    expect(immersiveFullscreenWindow && immersiveFullscreenWindow->isAncestorOf(playerPane),
        "dedicated fullscreen window should host the player pane");
    expect(videoHost->winId() == videoHostWindowId,
        "moving the player pane to fullscreen should preserve the native video host window");
    const bool fullscreenTimeoutInvoked = QMetaObject::invokeMethod(autoHideTimer, "timeout", Qt::DirectConnection);
    app.processEvents();
    expect(fullscreenTimeoutInvoked, "fullscreen auto-hide timeout should be invokable");
    expect(immersiveFullscreenWindow && immersiveFullscreenWindow->cursor().shape() == Qt::BlankCursor,
        "system fullscreen should hide the cursor on its own playback surface");
    QApplication::sendEvent(videoHost, &mouseMove);
    app.processEvents();
    expect(immersiveFullscreenWindow && immersiveFullscreenWindow->cursor().shape() != Qt::BlankCursor,
        "mouse activity should restore the cursor on the system fullscreen surface");
    windowStateRecorder.states.clear();
    fullscreenButton->click();
    expect(waitFor([]() {
        return !topLevelWidget(QStringLiteral("immersiveFullscreenWindow"));
    }), "leaving system fullscreen should close the dedicated fullscreen window");
    if (hasDistinctMaximizedWorkArea) {
        expect(window.isMaximized() && !window.isFullScreen(),
            "leaving system fullscreen should reveal the unchanged maximized main window");
    }
    expect(!windowStateRecorder.states.contains(Qt::WindowNoState),
        "leaving system fullscreen should not change the main window to normal state");
    expect(window.isAncestorOf(playerPane), "leaving system fullscreen should return the player pane to the main window");
    expect(videoHost->winId() == videoHostWindowId,
        "leaving fullscreen should preserve the native video host window");
    if (hasDistinctMaximizedWorkArea) {
        maximizeButton->click();
        const bool restoredNormalGeometry = waitFor([&window, normalWindowSize]() {
            return !window.isMaximized() && !window.isFullScreen() && window.size() == normalWindowSize;
        });
        expect(restoredNormalGeometry, "restore button should recover the original normal geometry after immersive system fullscreen");
        processEventsFor(300);
        expect(!window.isMaximized() && !window.isFullScreen() && window.size() == normalWindowSize,
            "restored normal geometry should survive pending native window events");
    }

    window.close();
    return failures == 0 ? 0 : 1;
}
