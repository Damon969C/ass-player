#include "subtitle_navigation.h"

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
