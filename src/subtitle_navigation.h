#pragma once

#include "subtitle_parser.h"

enum class SubtitleNavigationDirection {
    Previous,
    Next,
};

const SubtitleCue *findAdjacentSubtitleCue(
    const QVector<SubtitleCue> &cues,
    double positionSeconds,
    SubtitleNavigationDirection direction);
