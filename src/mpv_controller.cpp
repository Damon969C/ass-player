#include "mpv_controller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QThread>
#include <QVariant>
#include <algorithm>

MpvController::MpvController(QObject *parent) : QObject(parent) {}

MpvController::~MpvController()
{
    stop();
}

bool MpvController::openMedia(const QString &mpvPath, const QString &mediaPath, WId windowId, QString *error)
{
    stop();
    ipcName_ = uniqueIpcName();
#ifdef Q_OS_WIN
    const QString ipcArgument = QString(R"(\\.\pipe\%1)").arg(ipcName_);
#else
    const QString ipcArgument = ipcName_;
#endif
    QStringList arguments = {
        "--idle=no",
        "--force-window=yes",
        "--keep-open=yes",
        "--input-terminal=no",
        "--no-terminal",
        "--ontop=no",
        "--no-border",
        "--no-osc",
        "--sid=no",
        "--no-input-default-bindings",
        QString("--input-ipc-server=%1").arg(ipcArgument),
        QString("--wid=%1").arg(QString::number(static_cast<quintptr>(windowId))),
        mediaPath,
    };

    process_.start(mpvPath, arguments);
    if (!process_.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("启动 mpv 失败: %1").arg(process_.errorString());
        }
        return false;
    }

#ifdef Q_OS_WIN
    const QString socketName = ipcName_;
#else
    const QString socketName = ipcArgument;
#endif
    for (int attempt = 0; attempt < 80; ++attempt) {
        socket_.connectToServer(socketName);
        if (socket_.waitForConnected(50)) {
            return true;
        }
        socket_.abort();
        QThread::msleep(50);
    }
    stop();
    if (error) {
        *error = QStringLiteral("等待 mpv IPC 超时");
    }
    return false;
}

void MpvController::stop()
{
    socket_.abort();
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(1000);
    }
    nextRequestId_ = 0;
}

bool MpvController::addSubtitle(const QString &path, QString *error)
{
    return sendCommand(QJsonArray{"sub-add", path, "select"}, error);
}

bool MpvController::seekTo(double seconds, QString *error)
{
    return sendCommand(QJsonArray{"seek", seconds, "absolute", "exact"}, error)
        && sendCommand(QJsonArray{"set_property", "pause", false}, error);
}

bool MpvController::seekRelative(double seconds, QString *error)
{
    return sendCommand(QJsonArray{"seek", seconds, "relative", "exact"}, error);
}

bool MpvController::togglePlayPause(bool *paused, QString *error)
{
    if (!sendCommand(QJsonArray{"cycle", "pause"}, error)) {
        return false;
    }
    QThread::msleep(30);
    QJsonValue value;
    if (!getProperty("pause", &value, error)) {
        return false;
    }
    if (paused) {
        *paused = value.toBool(false);
    }
    return true;
}

bool MpvController::setSubtitleOverlay(bool enabled, QString *error)
{
    return sendCommand(QJsonArray{"set_property", "sub-visibility", enabled ? "yes" : "no"}, error);
}

bool MpvController::disablePictureSubtitle(QString *error)
{
    return sendCommand(QJsonArray{"set_property", "sid", "no"}, error);
}

bool MpvController::setAutoPictureSubtitle(QString *error)
{
    return sendCommand(QJsonArray{"set_property", "sid", "auto"}, error);
}

bool MpvController::selectPictureSubtitle(int mpvTrackId, QString *error)
{
    return sendCommand(QJsonArray{"set_property", "sid", mpvTrackId}, error);
}

bool MpvController::subtitleTracks(QVector<MpvSubtitleTrack> *tracks, QString *error)
{
    if (!tracks) {
        return false;
    }
    tracks->clear();

    QJsonValue value;
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (!getProperty(QStringLiteral("track-list"), &value, error)) {
            return false;
        }

        const QJsonArray trackList = value.toArray();
        QVector<MpvSubtitleTrack> parsedTracks;
        for (const QJsonValue &trackValue : trackList) {
            const QJsonObject trackObject = trackValue.toObject();
            if (trackObject.value(QStringLiteral("type")).toString() != QStringLiteral("sub")) {
                continue;
            }

            MpvSubtitleTrack track;
            track.id = trackObject.value(QStringLiteral("id")).toInt(-1);
            track.ffIndex = trackObject.contains(QStringLiteral("ff-index")) ? trackObject.value(QStringLiteral("ff-index")).toInt(-1) : -1;
            track.language = trackObject.value(QStringLiteral("lang")).toString();
            track.title = trackObject.value(QStringLiteral("title")).toString();
            track.codec = trackObject.value(QStringLiteral("codec")).toString();
            if (track.id >= 0) {
                parsedTracks.push_back(track);
            }
        }

        if (!parsedTracks.isEmpty() || attempt == 9) {
            *tracks = parsedTracks;
            return true;
        }
        QThread::msleep(40);
    }
    return true;
}

bool MpvController::setVolume(double volume, double *applied, QString *error)
{
    const double clamped = std::clamp(volume, 0.0, 100.0);
    if (!sendCommand(QJsonArray{"set_property", "volume", clamped}, error)
        || !sendCommand(QJsonArray{"set_property", "mute", false}, error)) {
        return false;
    }
    if (applied) {
        *applied = clamped;
    }
    return true;
}

bool MpvController::setMuted(bool muted, QString *error)
{
    return sendCommand(QJsonArray{"set_property", "mute", muted}, error);
}

bool MpvController::showOsd(const QString &text, QString *error)
{
    return sendCommand(QJsonArray{"set_property", "osd-align-x", "left"}, error)
        && sendCommand(QJsonArray{"set_property", "osd-align-y", "top"}, error)
        && sendCommand(QJsonArray{"set_property", "osd-font-size", 22}, error)
        && sendCommand(QJsonArray{"set_property", "osd-border-size", 1}, error)
        && sendCommand(QJsonArray{"set_property", "osd-color", "#38bdf8"}, error)
        && sendCommand(QJsonArray{"set_property", "osd-margin-x", 4}, error)
        && sendCommand(QJsonArray{"set_property", "osd-margin-y", 4}, error)
        && sendCommand(QJsonArray{"show-text", text, 1200}, error);
}

bool MpvController::playbackState(PlaybackState *state, QString *error)
{
    QJsonValue value;
    if (!getProperty("time-pos", &value, error)) return false;
    state->position = value.toDouble(0.0);
    if (!getProperty("duration", &value, error)) return false;
    state->duration = value.toDouble(0.0);
    if (!getProperty("pause", &value, error)) return false;
    state->paused = value.toBool(false);
    if (!getProperty("volume", &value, error)) return false;
    state->volume = value.toDouble(100.0);
    if (!getProperty("mute", &value, error)) return false;
    state->muted = value.toBool(false);
    return true;
}

bool MpvController::request(const QJsonObject &payload, QJsonObject *response, QString *error)
{
    if (socket_.state() != QLocalSocket::ConnectedState) {
        if (error) {
            *error = QStringLiteral("尚未启动播放器");
        }
        return false;
    }

    QJsonObject requestPayload = payload;
    const quint64 requestId = ++nextRequestId_;
    requestPayload.insert("request_id", QJsonValue::fromVariant(requestId));
    const QByteArray line = QJsonDocument(requestPayload).toJson(QJsonDocument::Compact) + '\n';
    if (socket_.write(line) != line.size() || !socket_.waitForBytesWritten(1000)) {
        if (error) {
            *error = QStringLiteral("发送 mpv 命令失败");
        }
        return false;
    }

    while (socket_.waitForReadyRead(3000)) {
        while (socket_.canReadLine()) {
            const QJsonDocument document = QJsonDocument::fromJson(socket_.readLine());
            const QJsonObject object = document.object();
            if (object.value("request_id").toInteger() == static_cast<qint64>(requestId)) {
                if (response) {
                    *response = object;
                }
                return true;
            }
        }
    }

    if (error) {
        *error = QStringLiteral("读取 mpv 响应超时");
    }
    return false;
}

bool MpvController::sendCommand(const QJsonArray &command, QString *error)
{
    QJsonObject response;
    if (!request(QJsonObject{{"command", command}}, &response, error)) {
        return false;
    }
    const QString commandError = response.value("error").toString("success");
    if (commandError != "success") {
        if (error) {
            *error = QStringLiteral("mpv 命令失败: %1").arg(commandError);
        }
        return false;
    }
    return true;
}

bool MpvController::getProperty(const QString &name, QJsonValue *value, QString *error)
{
    QJsonObject response;
    if (!request(QJsonObject{{"command", QJsonArray{"get_property", name}}}, &response, error)) {
        return false;
    }
    *value = response.value("data");
    return true;
}

QString MpvController::uniqueIpcName()
{
    return QString("subtitle-sidecar-player-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
}
