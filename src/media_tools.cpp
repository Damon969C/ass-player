#include "media_tools.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

QString MediaTools::findTool(const QString &name) const
{
#ifdef Q_OS_WIN
    const QString exe = name + ".exe";
#else
    const QString exe = name;
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("binaries/" + exe),
        QDir(appDir).filePath(exe),
        QDir(QDir::currentPath()).filePath("binaries/" + exe),
        QDir(QDir::currentPath()).filePath("src-tauri/binaries/" + exe),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return exe;
}

QVector<SubtitleTrack> MediaTools::probeSubtitleTracks(const QString &mediaPath, QString *error) const
{
    QByteArray output;
    const QStringList arguments = {"-v", "quiet", "-print_format", "json", "-show_streams", mediaPath};
    if (!runProcess(findTool("ffprobe"), arguments, &output, error)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(output);
    const QJsonArray streams = document.object().value("streams").toArray();
    QVector<SubtitleTrack> tracks;
    for (const QJsonValue &value : streams) {
        const QJsonObject stream = value.toObject();
        if (stream.value("codec_type").toString() != "subtitle") {
            continue;
        }
        const QJsonObject tags = stream.value("tags").toObject();
        SubtitleTrack track;
        track.index = stream.value("index").toInt();
        track.id = track.index;
        track.language = tags.value("language").toString();
        track.title = tags.value("title").toString();
        track.codec = stream.value("codec_name").toString();
        tracks.push_back(track);
    }
    return tracks;
}

void MediaTools::resetEmbeddedCache(const QString &mediaPath, const QVector<SubtitleTrack> &tracks)
{
    QMutexLocker locker(&cacheMutex_);
    cacheMediaKey_ = mediaHash(mediaPath);
    embeddedCache_.clear();
    trackExtensions_.clear();
    for (const SubtitleTrack &track : tracks) {
        trackExtensions_.insert(track.id, subtitleExtensionForCodec(track.codec));
    }
}

void MediaTools::prefetchEmbeddedSubtitles(const QString &mediaPath, const QVector<SubtitleTrack> &tracks)
{
    if (tracks.isEmpty()) {
        return;
    }

    QThread *thread = QThread::create([this, mediaPath, tracks]() {
        QVector<QPair<int, QString>> outputs;
        QStringList arguments = {"-y", "-nostdin", "-loglevel", "error", "-i", mediaPath};
        for (const SubtitleTrack &track : tracks) {
            EmbeddedSubtitleData cached;
            if (getCachedSubtitle(mediaPath, track.id, &cached)) {
                continue;
            }
            const QString extension = subtitleExtensionForCodec(track.codec);
            const QString outputPath = tempSubtitlePath(mediaPath, track.id, "batch-copy", extension);
            arguments << "-map" << QString("0:%1").arg(track.id)
                      << "-map_metadata" << "-1"
                      << "-map_chapters" << "-1"
                      << "-c:s" << "copy"
                      << outputPath;
            outputs.push_back({track.id, outputPath});
        }
        if (outputs.isEmpty()) {
            return;
        }

        QString error;
        if (!runProcess(findTool("ffmpeg"), arguments, nullptr, &error)) {
            for (const auto &output : outputs) {
                QFile::remove(output.second);
            }
            return;
        }
        for (const auto &output : outputs) {
            const QString extension = QFileInfo(output.second).suffix().toLower();
            EmbeddedSubtitleData data;
            if (readValidatedSubtitle(output.second, extension, &data)) {
                insertCachedSubtitle(mediaPath, output.first, data);
            }
            QFile::remove(output.second);
        }
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

EmbeddedSubtitleLoadResult MediaTools::loadEmbeddedSubtitle(const QString &mediaPath, int trackId, QString *error)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    EmbeddedSubtitleLoadResult result;
    EmbeddedSubtitleData data;

    if (getCachedSubtitle(mediaPath, trackId, &data)) {
        result.extractStrategy = "memory-cache";
    } else {
        QElapsedTimer extractTimer;
        extractTimer.start();
        QString copyExtension;
        {
            QMutexLocker locker(&cacheMutex_);
            copyExtension = trackExtensions_.value(trackId, "srt");
        }
        const QString copyPath = tempSubtitlePath(mediaPath, trackId, "copy", copyExtension);
        if (extractEmbeddedSubtitle(mediaPath, trackId, copyPath, true, nullptr)
            && readValidatedSubtitle(copyPath, copyExtension, &data)) {
            QFile::remove(copyPath);
            insertCachedSubtitle(mediaPath, trackId, data);
            result.extractStrategy = "copy";
        } else {
            QFile::remove(copyPath);
            const QString srtPath = tempSubtitlePath(mediaPath, trackId, "srt", "srt");
            if (!extractEmbeddedSubtitle(mediaPath, trackId, srtPath, false, error)
                || !readValidatedSubtitle(srtPath, "srt", &data)) {
                QFile::remove(srtPath);
                if (error && error->isEmpty()) {
                    *error = QStringLiteral("抽取内嵌字幕失败，可能该轨道格式暂不支持当前导出方式");
                }
                return {};
            }
            QFile::remove(srtPath);
            insertCachedSubtitle(mediaPath, trackId, data);
            result.extractStrategy = "srt";
        }
        result.ffmpegExtractMs = extractTimer.elapsed();
    }

    QElapsedTimer parseTimer;
    parseTimer.start();
    result.cues = SubtitleParser::parseBytes(data.bytes, data.extension, error);
    result.parseMs = parseTimer.elapsed();
    if (result.cues.isEmpty()) {
        return {};
    }

    const QString runtimePath = tempSubtitlePath(mediaPath, trackId, "runtime", data.extension);
    QFile file(runtimePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(data.bytes) != data.bytes.size()) {
        if (error) {
            *error = QStringLiteral("准备字幕文件失败: %1").arg(file.errorString());
        }
        return {};
    }
    {
        QMutexLocker locker(&runtimeFilesMutex_);
        runtimeSubtitleFiles_.push_back(runtimePath);
    }
    result.runtimePath = runtimePath;
    result.backendTotalMs = totalTimer.elapsed();
    return result;
}

void MediaTools::cleanupRuntimeSubtitleFiles()
{
    QVector<QString> files;
    {
        QMutexLocker locker(&runtimeFilesMutex_);
        files = runtimeSubtitleFiles_;
        runtimeSubtitleFiles_.clear();
    }
    for (const QString &path : std::as_const(files)) {
        QFile::remove(path);
    }
}

void MediaTools::discardRuntimeSubtitleFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    {
        QMutexLocker locker(&runtimeFilesMutex_);
        runtimeSubtitleFiles_.removeAll(path);
    }
    QFile::remove(path);
}

QString MediaTools::subtitleExtensionForCodec(const QString &codec)
{
    const QString normalized = codec.toLower();
    if (normalized == "ass" || normalized == "ssa") {
        return "ass";
    }
    if (normalized == "webvtt") {
        return "vtt";
    }
    return "srt";
}

QString MediaTools::tempSubtitlePath(const QString &mediaPath, int trackId, const QString &label, const QString &extension)
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const quint64 hash = fnv1a64(QString("%1-%2").arg(mediaHash(mediaPath)).arg(trackId).toUtf8());
    const qint64 nanos = QDateTime::currentMSecsSinceEpoch();
    return QDir(tempDir).filePath(QString("subtitle-sidecar-player-%1-%2-%3-%4-%5.%6")
        .arg(hash, 16, 16, QLatin1Char('0'))
        .arg(trackId)
        .arg(label)
        .arg(QCoreApplication::applicationPid())
        .arg(nanos)
        .arg(extension));
}

quint64 MediaTools::fnv1a64(const QByteArray &bytes)
{
    quint64 hash = 0xcbf29ce484222325ULL;
    for (const char byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

quint64 MediaTools::mediaHash(const QString &mediaPath)
{
    const QFileInfo info(mediaPath);
    const QString key = QString("%1-%2").arg(info.absoluteFilePath()).arg(info.lastModified().toSecsSinceEpoch());
    return fnv1a64(key.toUtf8());
}

bool MediaTools::runProcess(const QString &program, const QStringList &arguments, QByteArray *stdoutData, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("启动进程失败: %1").arg(program);
        }
        return false;
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = detail.isEmpty() ? QStringLiteral("进程执行失败: %1").arg(program) : detail;
        }
        return false;
    }
    if (stdoutData) {
        *stdoutData = process.readAllStandardOutput();
    }
    return true;
}

bool MediaTools::readValidatedSubtitle(const QString &path, const QString &extension, EmbeddedSubtitleData *data)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    QString error;
    if (SubtitleParser::parseBytes(bytes, extension, &error).isEmpty()) {
        return false;
    }
    data->extension = extension;
    data->bytes = bytes;
    return true;
}

bool MediaTools::extractEmbeddedSubtitle(const QString &mediaPath, int trackId, const QString &outputPath, bool copyCodec, QString *error) const
{
    QStringList arguments = {"-y", "-nostdin", "-loglevel", "error", "-i", mediaPath,
                             "-map", QString("0:%1").arg(trackId),
                             "-map_metadata", "-1", "-map_chapters", "-1",
                             "-c:s", copyCodec ? "copy" : "srt", outputPath};
    return runProcess(findTool("ffmpeg"), arguments, nullptr, error);
}

bool MediaTools::insertCachedSubtitle(const QString &mediaPath, int trackId, const EmbeddedSubtitleData &data)
{
    QMutexLocker locker(&cacheMutex_);
    if (cacheMediaKey_ != mediaHash(mediaPath) || embeddedCache_.contains(trackId)) {
        return false;
    }
    embeddedCache_.insert(trackId, data);
    return true;
}

bool MediaTools::getCachedSubtitle(const QString &mediaPath, int trackId, EmbeddedSubtitleData *data) const
{
    QMutexLocker locker(&cacheMutex_);
    if (cacheMediaKey_ != mediaHash(mediaPath) || !embeddedCache_.contains(trackId)) {
        return false;
    }
    *data = embeddedCache_.value(trackId);
    return true;
}
