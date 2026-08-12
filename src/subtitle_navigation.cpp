#include "subtitle_navigation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
qsizetype findActiveCueIndex(const QVector<SubtitleCue> &cues, double positionSeconds)
{
    for (qsizetype index = 0; index < cues.size(); ++index) {
        const SubtitleCue &cue = cues[index];
        const double end = cue.endSeconds >= 0.0
            ? cue.endSeconds
            : std::numeric_limits<double>::infinity();
        if (positionSeconds >= cue.startSeconds && positionSeconds < end) {
            return index;
        }
    }
    return -1;
}
}

const SubtitleCue *findAdjacentSubtitleCue(
    const QVector<SubtitleCue> &cues,
    double positionSeconds,
    SubtitleNavigationDirection direction)
{
    if (cues.isEmpty() || !std::isfinite(positionSeconds)) {
        return nullptr;
    }

    const qsizetype activeIndex = findActiveCueIndex(cues, positionSeconds);
    if (activeIndex >= 0) {
        const qsizetype targetIndex = direction == SubtitleNavigationDirection::Previous
            ? activeIndex - 1
            : activeIndex + 1;
        return targetIndex >= 0 && targetIndex < cues.size() ? &cues[targetIndex] : nullptr;
    }

    if (direction == SubtitleNavigationDirection::Previous) {
        for (qsizetype index = cues.size(); index > 0; --index) {
            if (cues[index - 1].startSeconds < positionSeconds) {
                return &cues[index - 1];
            }
        }
        return nullptr;
    }

    for (const SubtitleCue &cue : cues) {
        if (cue.startSeconds > positionSeconds) {
            return &cue;
        }
    }
    return nullptr;
}

const SubtitleCue *findSubtitleCueById(const QVector<SubtitleCue> &cues, int cueId)
{
    for (const SubtitleCue &cue : cues) {
        if (cue.id == cueId) {
            return &cue;
        }
    }
    return nullptr;
}

const SubtitleCue *findAdjacentSubtitleCueById(
    const QVector<SubtitleCue> &cues,
    int cueId,
    SubtitleNavigationDirection direction)
{
    for (qsizetype index = 0; index < cues.size(); ++index) {
        if (cues[index].id != cueId) {
            continue;
        }
        const qsizetype targetIndex = direction == SubtitleNavigationDirection::Previous
            ? index - 1
            : index + 1;
        return targetIndex >= 0 && targetIndex < cues.size() ? &cues[targetIndex] : nullptr;
    }
    return nullptr;
}

double subtitleCuePlaybackEnd(
    const QVector<SubtitleCue> &cues,
    int cueId,
    double mediaDurationSeconds)
{
    for (qsizetype index = 0; index < cues.size(); ++index) {
        const SubtitleCue &cue = cues[index];
        if (cue.id != cueId) {
            continue;
        }

        double end = std::isfinite(cue.endSeconds) && cue.endSeconds > cue.startSeconds
            ? cue.endSeconds
            : -1.0;
        if (end < 0.0) {
            for (qsizetype nextIndex = index + 1; nextIndex < cues.size(); ++nextIndex) {
                if (std::isfinite(cues[nextIndex].startSeconds)
                    && cues[nextIndex].startSeconds > cue.startSeconds) {
                    end = cues[nextIndex].startSeconds;
                    break;
                }
            }
        }
        if (std::isfinite(mediaDurationSeconds) && mediaDurationSeconds > cue.startSeconds) {
            end = end > cue.startSeconds ? std::min(end, mediaDurationSeconds) : mediaDurationSeconds;
        }
        return end > cue.startSeconds ? end : -1.0;
    }
    return -1.0;
}
