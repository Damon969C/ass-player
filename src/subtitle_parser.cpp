#include "subtitle_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringConverter>
#include <algorithm>
#include <cmath>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
struct AssStyle {
    int alignment = 2;
    int marginV = 0;
};

struct AssDialogue {
    double start = 0.0;
    double end = -1.0;
    QString text;
    QString style;
    int layer = 0;
    int order = 0;
};

QString decodeWithName(const QByteArray &bytes, const char *name)
{
#ifdef Q_OS_WIN
    const QString codecName = QString::fromLatin1(name).toLower();
    const UINT codePage = codecName == "gb18030" ? 54936 : codecName == "shift-jis" ? 932 : CP_UTF8;
    const int length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), nullptr, 0);
    if (length <= 0) {
        return QString();
    }
    QString output(length, Qt::Uninitialized);
    MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), reinterpret_cast<LPWSTR>(output.data()), length);
    return output;
#else
    Q_UNUSED(name);
    return QString::fromLocal8Bit(bytes);
#endif
}

QStringList splitBlocks(const QString &content)
{
    return content.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
}

QStringList parseAssFormat(const QString &value)
{
    QStringList fields;
    for (const QString &field : value.split(',')) {
        fields.push_back(field.trimmed().toLower());
    }
    return fields;
}

int assFieldIndex(const QStringList &fields, const QString &name)
{
    return fields.indexOf(name.toLower());
}

QStringList splitAssFields(const QString &value, int fieldCount)
{
    if (fieldCount <= 1) {
        return {value.trimmed()};
    }
    return value.trimmed().split(',', Qt::KeepEmptyParts).mid(0, fieldCount - 1)
        + QStringList{value.section(',', fieldCount - 1).trimmed()};
}

int assVisualRank(const AssDialogue &dialogue, const QHash<QString, AssStyle> &styles)
{
    const AssStyle style = styles.value(dialogue.style, AssStyle{});
    if (style.alignment >= 7 && style.alignment <= 9) {
        return style.marginV;
    }
    if (style.alignment >= 4 && style.alignment <= 6) {
        return 5000 + style.marginV;
    }
    return 10000 - style.marginV;
}

bool sameAssTime(const AssDialogue &left, const AssDialogue &right)
{
    constexpr double epsilon = 0.015;
    const bool sameStart = std::abs(left.start - right.start) <= epsilon;
    const bool sameEnd = (left.end < 0.0 && right.end < 0.0) || std::abs(left.end - right.end) <= epsilon;
    return sameStart && sameEnd;
}

bool sameCueTime(const SubtitleCue &left, const SubtitleCue &right)
{
    constexpr double epsilon = 0.015;
    const bool sameStart = std::abs(left.startSeconds - right.startSeconds) <= epsilon;
    const bool sameEnd = (left.endSeconds < 0.0 && right.endSeconds < 0.0) || std::abs(left.endSeconds - right.endSeconds) <= epsilon;
    return sameStart && sameEnd;
}

QVector<SubtitleCue> mergeSameTimeCues(QVector<SubtitleCue> cues)
{
    if (cues.size() < 2) {
        return cues;
    }

    std::stable_sort(cues.begin(), cues.end(), [](const SubtitleCue &left, const SubtitleCue &right) {
        if (left.startSeconds != right.startSeconds) return left.startSeconds < right.startSeconds;
        if (left.endSeconds != right.endSeconds) return left.endSeconds < right.endSeconds;
        return left.id < right.id;
    });

    QVector<SubtitleCue> merged;
    QVector<SubtitleCue> group;
    auto flushGroup = [&]() {
        if (group.isEmpty()) {
            return;
        }

        QStringList lines;
        for (const SubtitleCue &cue : std::as_const(group)) {
            const QString text = cue.text.trimmed();
            if (!text.isEmpty()) {
                lines.push_back(text);
            }
        }

        if (!lines.isEmpty()) {
            merged.push_back({static_cast<int>(merged.size()), group.first().startSeconds, group.first().endSeconds, lines.join(QStringLiteral("\n")), group.first().sourceType});
        }
        group.clear();
    };

    for (const SubtitleCue &cue : std::as_const(cues)) {
        if (!group.isEmpty() && !sameCueTime(group.first(), cue)) {
            flushGroup();
        }
        group.push_back(cue);
    }
    flushGroup();
    return merged;
}
}

QVector<SubtitleCue> SubtitleParser::parseFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("打开字幕失败: %1").arg(file.errorString());
        }
        return {};
    }

    const QString extension = QFileInfo(path).suffix().toLower();
    return parseBytes(file.readAll(), extension, error);
}

QVector<SubtitleCue> SubtitleParser::parseBytes(const QByteArray &bytes, const QString &extension, QString *error)
{
    QString content = decodeBytes(bytes).replace("\r\n", "\n").replace('\r', '\n');
    QVector<SubtitleCue> cues;
    const QString normalized = extension.toLower();
    if (normalized == "ass" || normalized == "ssa") {
        cues = parseAss(content);
    } else if (normalized == "lrc") {
        cues = parseLrc(content);
    } else {
        cues = parseSrtOrVtt(content, normalized == "vtt" ? "vtt" : "srt");
    }

    if (cues.isEmpty() && error) {
        *error = QStringLiteral("未解析到有效字幕条目");
    }
    return mergeSameTimeCues(cues);
}

QString SubtitleParser::decodeBytes(const QByteArray &bytes)
{
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(bytes.mid(3));
    }
    if (bytes.startsWith("\xFF\xFE")) {
        return decodeWithName(bytes.mid(2), "UTF-16LE");
    }
    if (bytes.startsWith("\xFE\xFF")) {
        return decodeWithName(bytes.mid(2), "UTF-16BE");
    }

    const QString utf8 = QString::fromUtf8(bytes);
    if (!utf8.contains(QChar::ReplacementCharacter)) {
        return utf8;
    }

    for (const char *name : {"GB18030", "Shift-JIS"}) {
        const QString decoded = decodeWithName(bytes, name);
        if (!decoded.isEmpty() && !decoded.contains(QChar::ReplacementCharacter)) {
            return decoded;
        }
    }
    const QString gb18030 = decodeWithName(bytes, "GB18030");
    return gb18030.isEmpty() ? QString::fromLocal8Bit(bytes) : gb18030;
}

QVector<SubtitleCue> SubtitleParser::parseSrtOrVtt(const QString &content, const QString &sourceType)
{
    QVector<SubtitleCue> cues;
    for (const QString &block : splitBlocks(content)) {
        const QStringList lines = block.split('\n', Qt::SkipEmptyParts);
        int timingIndex = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains("-->")) {
                timingIndex = i;
                break;
            }
        }
        if (timingIndex < 0) {
            continue;
        }
        const QStringList timing = lines[timingIndex].split("-->");
        bool startOk = false;
        bool endOk = false;
        const double start = parseTimestamp(timing.value(0), &startOk);
        const double end = parseTimestamp(timing.value(1), &endOk);
        QStringList textLines = lines.mid(timingIndex + 1);
        QString text = textLines.join('\n').replace("<br>", "\n").trimmed();
        if (!startOk || text.isEmpty()) {
            continue;
        }
        cues.push_back({static_cast<int>(cues.size()), start, endOk ? end : -1.0, text, sourceType});
    }
    return cues;
}

QVector<SubtitleCue> SubtitleParser::parseAss(const QString &content)
{
    QHash<QString, AssStyle> styles;
    QVector<AssDialogue> dialogues;
    QString section;
    QStringList styleFields;
    QStringList eventFields = {"layer", "start", "end", "style", "name", "marginl", "marginr", "marginv", "effect", "text"};

    for (const QString &line : content.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            section = trimmed.toLower();
            continue;
        }
        if (section == "[v4+ styles]" || section == "[v4 styles]") {
            if (trimmed.startsWith("Format:", Qt::CaseInsensitive)) {
                styleFields = parseAssFormat(trimmed.mid(trimmed.indexOf(':') + 1));
            } else if (trimmed.startsWith("Style:", Qt::CaseInsensitive) && !styleFields.isEmpty()) {
                const QStringList parts = splitAssFields(trimmed.mid(trimmed.indexOf(':') + 1), styleFields.size());
                const int nameIndex = assFieldIndex(styleFields, "name");
                if (nameIndex >= 0 && nameIndex < parts.size()) {
                    AssStyle style;
                    const int alignmentIndex = assFieldIndex(styleFields, "alignment");
                    const int marginIndex = assFieldIndex(styleFields, "marginv");
                    if (alignmentIndex >= 0 && alignmentIndex < parts.size()) {
                        style.alignment = parts[alignmentIndex].trimmed().toInt();
                    }
                    if (marginIndex >= 0 && marginIndex < parts.size()) {
                        style.marginV = parts[marginIndex].trimmed().toInt();
                    }
                    styles.insert(parts[nameIndex].trimmed(), style);
                }
            }
        } else if (section == "[events]") {
            if (trimmed.startsWith("Format:", Qt::CaseInsensitive)) {
                eventFields = parseAssFormat(trimmed.mid(trimmed.indexOf(':') + 1));
            } else if (trimmed.startsWith("Dialogue:", Qt::CaseInsensitive)) {
                const QStringList parts = splitAssFields(trimmed.mid(trimmed.indexOf(':') + 1), eventFields.size());
                const int startIndex = assFieldIndex(eventFields, "start");
                const int endIndex = assFieldIndex(eventFields, "end");
                const int textIndex = assFieldIndex(eventFields, "text");
                if (startIndex < 0 || endIndex < 0 || textIndex < 0 || textIndex >= parts.size()) {
                    continue;
                }
                bool startOk = false;
                bool endOk = false;
                AssDialogue dialogue;
                dialogue.start = parseTimestamp(parts[startIndex], &startOk);
                dialogue.end = parseTimestamp(parts[endIndex], &endOk);
                dialogue.text = cleanAssText(parts[textIndex]);
                dialogue.order = dialogues.size();
                const int styleIndex = assFieldIndex(eventFields, "style");
                const int layerIndex = assFieldIndex(eventFields, "layer");
                if (styleIndex >= 0 && styleIndex < parts.size()) {
                    dialogue.style = parts[styleIndex].trimmed();
                }
                if (layerIndex >= 0 && layerIndex < parts.size()) {
                    dialogue.layer = parts[layerIndex].trimmed().toInt();
                }
                if (startOk && !dialogue.text.isEmpty()) {
                    if (!endOk) {
                        dialogue.end = -1.0;
                    }
                    dialogues.push_back(dialogue);
                }
            }
        }
    }

    std::sort(dialogues.begin(), dialogues.end(), [](const AssDialogue &left, const AssDialogue &right) {
        if (left.start != right.start) return left.start < right.start;
        if (left.end != right.end) return left.end < right.end;
        return left.order < right.order;
    });

    QVector<SubtitleCue> cues;
    QVector<AssDialogue> group;
    auto flushGroup = [&]() {
        if (group.isEmpty()) {
            return;
        }
        std::sort(group.begin(), group.end(), [&](const AssDialogue &left, const AssDialogue &right) {
            const int rankLeft = assVisualRank(left, styles);
            const int rankRight = assVisualRank(right, styles);
            if (rankLeft != rankRight) return rankLeft < rankRight;
            if (left.layer != right.layer) return left.layer < right.layer;
            return left.order < right.order;
        });
        QStringList lines;
        for (const AssDialogue &dialogue : group) {
            lines.push_back(dialogue.text);
        }
        const QString text = lines.join('\n').trimmed();
        if (!text.isEmpty()) {
            cues.push_back({static_cast<int>(cues.size()), group.first().start, group.first().end, text, "ass"});
        }
        group.clear();
    };

    for (const AssDialogue &dialogue : dialogues) {
        if (!group.isEmpty() && !sameAssTime(group.first(), dialogue)) {
            flushGroup();
        }
        group.push_back(dialogue);
    }
    flushGroup();
    return cues;
}

QVector<SubtitleCue> SubtitleParser::parseLrc(const QString &content)
{
    QVector<QPair<double, QString>> entries;
    const QRegularExpression tagPattern(R"(\[([^\]]+)\])");
    for (const QString &line : content.split('\n')) {
        QVector<double> times;
        auto matchIterator = tagPattern.globalMatch(line);
        int lastTagEnd = 0;
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            bool ok = false;
            const double seconds = parseTimestamp(match.captured(1), &ok);
            if (ok) {
                times.push_back(seconds);
            }
            lastTagEnd = match.capturedEnd();
        }
        const QString text = line.mid(lastTagEnd).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        for (double time : times) {
            entries.push_back({time, text});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    QVector<SubtitleCue> cues;
    for (int i = 0; i < entries.size(); ++i) {
        cues.push_back({i, entries[i].first, i + 1 < entries.size() ? entries[i + 1].first : -1.0, entries[i].second, "lrc"});
    }
    return cues;
}

double SubtitleParser::parseTimestamp(const QString &value, bool *ok)
{
    const QString normalized = value.trimmed().replace(',', '.');
    const QStringList parts = normalized.split(':');
    bool localOk = false;
    double seconds = 0.0;
    if (parts.size() == 3) {
        seconds = parts[0].toDouble(&localOk) * 3600.0;
        bool minutesOk = false;
        bool secondsOk = false;
        seconds += parts[1].toDouble(&minutesOk) * 60.0 + parts[2].toDouble(&secondsOk);
        localOk = localOk && minutesOk && secondsOk;
    } else if (parts.size() == 2) {
        seconds = parts[0].toDouble(&localOk) * 60.0;
        bool secondsOk = false;
        seconds += parts[1].toDouble(&secondsOk);
        localOk = localOk && secondsOk;
    }
    if (ok) {
        *ok = localOk;
    }
    return seconds;
}

QString SubtitleParser::cleanAssText(const QString &value)
{
    QString output;
    bool inTag = false;
    const QString normalized = QString(value).replace("\\N", "\n").replace("\\n", "\n");
    for (const QChar character : normalized) {
        if (character == '{') {
            inTag = true;
        } else if (character == '}') {
            inTag = false;
        } else if (!inTag) {
            output.append(character);
        }
    }
    return output.trimmed();
}
