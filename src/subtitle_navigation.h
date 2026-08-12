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

const SubtitleCue *findSubtitleCueById(const QVector<SubtitleCue> &cues, int cueId);

const SubtitleCue *findAdjacentSubtitleCueById(
    const QVector<SubtitleCue> &cues,
    int cueId,
    SubtitleNavigationDirection direction);

double subtitleCuePlaybackEnd(
    const QVector<SubtitleCue> &cues,
    int cueId,
    double mediaDurationSeconds);
