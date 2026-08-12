#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVector>
#include <QtGui/qwindowdefs.h>

struct PlaybackState {
    double position = 0.0;
    double duration = 0.0;
    bool paused = true;
    double volume = 100.0;
    bool muted = false;
};

struct MpvSubtitleTrack {
    int id = -1;
    int ffIndex = -1;
    QString language;
    QString title;
    QString codec;
};

class MpvController : public QObject {
    Q_OBJECT

public:
    explicit MpvController(QObject *parent = nullptr);
    ~MpvController() override;

    bool openMedia(const QString &mpvPath, const QString &mediaPath, WId windowId, QString *error = nullptr);
    void stop();
    bool addSubtitle(const QString &path, QString *error = nullptr);
    bool seekAbsolute(double seconds, QString *error = nullptr);
    bool seekTo(double seconds, QString *error = nullptr);
    bool togglePlayPause(bool *paused, QString *error = nullptr);
    bool setPaused(bool paused, QString *error = nullptr);
    bool setSubtitleOverlay(bool enabled, QString *error = nullptr);
    bool disablePictureSubtitle(QString *error = nullptr);
    bool setAutoPictureSubtitle(QString *error = nullptr);
    bool selectPictureSubtitle(int mpvTrackId, QString *error = nullptr);
    bool subtitleTracks(QVector<MpvSubtitleTrack> *tracks, QString *error = nullptr);
    bool setVolume(double volume, double *applied = nullptr, QString *error = nullptr);
    bool setMuted(bool muted, QString *error = nullptr);
    bool showOsd(const QString &text, QString *error = nullptr);
    bool playbackState(PlaybackState *state, QString *error = nullptr);

private:
    bool request(const QJsonObject &payload, QJsonObject *response, QString *error);
    bool sendCommand(const QJsonArray &command, QString *error);
    bool getProperty(const QString &name, QJsonValue *value, QString *error);
    static QString uniqueIpcName();

    QProcess process_;
    QLocalSocket socket_;
    QString ipcName_;
    quint64 nextRequestId_ = 0;
};
