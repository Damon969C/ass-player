#pragma once

#include "media_tools.h"
#include "mpv_controller.h"
#include "subtitle_navigation.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QThread>
#include <QTimer>
#include <QToolButton>

class QScrollBar;
class QPropertyAnimation;
class QHBoxLayout;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void buildUi();
    void connectSignals();
    void openVideoDialog();
    void openSubtitleDialog();
    void openMedia(const QString &path);
    void loadExternalSubtitle(const QString &path);
    void finishExternalSubtitleLoad(int requestId, const QString &mediaPath, const QString &subtitlePath, const QVector<SubtitleCue> &cues, const QString &error);
    void selectEmbeddedSubtitle(int index);
    void finishEmbeddedSubtitleLoad(int requestId, const QString &mediaPath, int trackId, EmbeddedSubtitleLoadResult result, const QString &error);
    void selectPictureSubtitle(int index);
    void populatePictureSubtitleSelect();
    void setPictureSubtitleExternalFollowing();
    void setPictureSubtitleUnavailable(const QString &label);
    void updatePictureSubtitleSelectVisibility();
    void handleDroppedPath(const QString &path);
    bool handlePlaybackKey(QKeyEvent *event);
    void togglePlayPause(bool showHint);
    void seekToAdjacentSubtitle(SubtitleNavigationDirection direction);
    void seekTo(double seconds, bool scroll);
    void applyVolume(int volume, bool showHint);
    void setMuted(bool muted);
    void syncSubtitleOverlay();
    void pollPlaybackState();
    void setCues(const QVector<SubtitleCue> &cues, const QString &message);
    void setActiveCue(int cueId, bool scroll);
    void suppressSubtitleAutoScrollForManualNavigation();
    void stopSubtitleScrollAnimation();
    void refreshSubtitleListLayoutForWidthChange();
    const SubtitleCue *findActiveCue(double seconds) const;
    const SubtitleCue *findSeekCue(double seconds) const;
    void updateTimeline(double position, double duration);
    void updateVolumeUi(double volume, bool muted);
    void setStatus(const QString &message);
    void showVolumeOsd(double volume);
    void showOsd(const QString &text);
    void toggleImmersivePlayback();
    void enterImmersivePlayback();
    void exitImmersivePlayback();
    void showImmersiveControls();
    void hideImmersiveControls();
    void positionImmersiveControls();
    void setImmersiveSurfaceStyle(bool immersive);
    bool isEventFromThisWindow(QObject *object) const;
    void toggleMaximized();
    void updateWindowControlGeometry();
    void updateWindowControlVisibility();
    void updateMaximizeButton();
    Qt::Edges resizeEdgesAt(const QPoint &position) const;
    bool isMoveZoneAt(const QPoint &position) const;
    bool isInteractiveAt(const QPoint &position) const;
    void restoreSettings();
    void saveSettings();
    static bool isVideoPath(const QString &path);
    static bool isSubtitlePath(const QString &path);
    static QString subtitleTrackLabel(const SubtitleTrack &track);
    static QString basename(const QString &path);
    static QString formatTime(double seconds);
    static QString formatClock(double seconds);

    QHBoxLayout *rootLayout_ = nullptr;
    QVBoxLayout *playerLayout_ = nullptr;
    QVBoxLayout *videoShellLayout_ = nullptr;
    QWidget *playerPane_ = nullptr;
    QWidget *toolbarStrip_ = nullptr;
    QWidget *videoShell_ = nullptr;
    QWidget *videoHost_ = nullptr;
    QWidget *controlStrip_ = nullptr;
    QWidget *subtitlePane_ = nullptr;
    QPushButton *openVideoButton_ = nullptr;
    QPushButton *openSubtitleButton_ = nullptr;
    QPushButton *playPauseButton_ = nullptr;
    QCheckBox *overlayToggle_ = nullptr;
    QComboBox *pictureSubtitleSelect_ = nullptr;
    QWidget *pictureSubtitleSelectPopupViewport_ = nullptr;
    QComboBox *embeddedSelect_ = nullptr;
    QWidget *embeddedSelectPopupViewport_ = nullptr;
    QCheckBox *timingLogToggle_ = nullptr;
    QLabel *mediaStatus_ = nullptr;
    QLabel *subtitleStatus_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QSlider *progressSlider_ = nullptr;
    QPushButton *muteButton_ = nullptr;
    QSlider *volumeSlider_ = nullptr;
    QPushButton *fullscreenButton_ = nullptr;
    QListWidget *subtitleList_ = nullptr;
    QWidget *subtitleListViewport_ = nullptr;
    QScrollBar *subtitleListScrollBar_ = nullptr;
    QPropertyAnimation *subtitleScrollAnimation_ = nullptr;
    QLabel *loadingOverlay_ = nullptr;
    QWidget *windowControls_ = nullptr;
    QToolButton *minimizeButton_ = nullptr;
    QToolButton *maximizeButton_ = nullptr;
    QToolButton *closeButton_ = nullptr;

    MpvController mpv_;
    MediaTools mediaTools_;
    QSettings settings_;
    QTimer stateTimer_;
    QTimer windowControlRevealTimer_;
    QTimer *immersiveControlsTimer_ = nullptr;
    QVector<SubtitleCue> cues_;
    QVector<SubtitleTrack> tracks_;
    QVector<MpvSubtitleTrack> pictureSubtitleTracks_;
    QVector<QThread *> subtitleLoadThreads_;
    QString currentMediaPath_;
    PlaybackState lastState_;
    int activeCueId_ = -1;
    int subtitleListViewportWidth_ = -1;
    qint64 suppressAutoScrollUntil_ = 0;
    int embeddedSubtitleLoadRequestId_ = 0;
    bool mediaLoaded_ = false;
    bool immersiveMode_ = false;
    bool immersiveUsesWindowFullscreen_ = false;
};
