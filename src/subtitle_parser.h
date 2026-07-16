#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct SubtitleCue {
    int id = 0;
    double startSeconds = 0.0;
    double endSeconds = -1.0;
    QString text;
    QString sourceType;
};

class SubtitleParser {
public:
    static QVector<SubtitleCue> parseFile(const QString &path, QString *error = nullptr);
    static QVector<SubtitleCue> parseBytes(const QByteArray &bytes, const QString &extension, QString *error = nullptr);
    static QString decodeBytes(const QByteArray &bytes);

private:
    static QVector<SubtitleCue> parseSrtOrVtt(const QString &content, const QString &sourceType);
    static QVector<SubtitleCue> parseAss(const QString &content);
    static QVector<SubtitleCue> parseLrc(const QString &content);
    static double parseTimestamp(const QString &value, bool *ok = nullptr);
    static QString cleanAssText(const QString &value);
};
