#pragma once

#include "subtitle_parser.h"

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

struct SubtitleTrack {
    int id = 0;
    int index = 0;
    QString language;
    QString title;
    QString codec;
};

struct EmbeddedSubtitleData {
    QString extension;
    QByteArray bytes;
};

struct EmbeddedSubtitleLoadResult {
    QVector<SubtitleCue> cues;
    qint64 ffmpegExtractMs = 0;
    qint64 parseMs = 0;
    qint64 mpvIpcMs = 0;
    qint64 backendTotalMs = 0;
    QString extractStrategy;
    QString runtimePath;
};

class MediaTools {
public:
    QString findTool(const QString &name) const;
    QVector<SubtitleTrack> probeSubtitleTracks(const QString &mediaPath, QString *error = nullptr) const;
    void resetEmbeddedCache(const QString &mediaPath, const QVector<SubtitleTrack> &tracks);
    void prefetchEmbeddedSubtitles(const QString &mediaPath, const QVector<SubtitleTrack> &tracks);
    EmbeddedSubtitleLoadResult loadEmbeddedSubtitle(const QString &mediaPath, int trackId, QString *error = nullptr);
    void cleanupRuntimeSubtitleFiles();
    void discardRuntimeSubtitleFile(const QString &path);

private:
    static QString subtitleExtensionForCodec(const QString &codec);
    static QString tempSubtitlePath(const QString &mediaPath, int trackId, const QString &label, const QString &extension);
    static quint64 fnv1a64(const QByteArray &bytes);
    static quint64 mediaHash(const QString &mediaPath);
    static bool runProcess(const QString &program, const QStringList &arguments, QByteArray *stdoutData, QString *error);
    static bool readValidatedSubtitle(const QString &path, const QString &extension, EmbeddedSubtitleData *data);
    bool extractEmbeddedSubtitle(const QString &mediaPath, int trackId, const QString &outputPath, bool copyCodec, QString *error) const;
    bool insertCachedSubtitle(const QString &mediaPath, int trackId, const EmbeddedSubtitleData &data);
    bool getCachedSubtitle(const QString &mediaPath, int trackId, EmbeddedSubtitleData *data) const;

    mutable QMutex cacheMutex_;
    quint64 cacheMediaKey_ = 0;
    QHash<int, EmbeddedSubtitleData> embeddedCache_;
    QHash<int, QString> trackExtensions_;
    mutable QMutex runtimeFilesMutex_;
    QVector<QString> runtimeSubtitleFiles_;
};
