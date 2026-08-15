#include "subtitle_navigation.h"

#include <QTextStream>

#include <cmath>
#include <limits>

namespace {
int failures = 0;

void expectCue(
    const QVector<SubtitleCue> &cues,
    double positionSeconds,
    SubtitleNavigationDirection direction,
    int expectedCueId,
    const char *scenario)
{
    const SubtitleCue *cue = findAdjacentSubtitleCue(cues, positionSeconds, direction);
    const int actualCueId = cue ? cue->id : -1;
    if (actualCueId != expectedCueId) {
        QTextStream(stderr) << scenario << ": expected cue " << expectedCueId
                            << ", got " << actualCueId << '\n';
        ++failures;
    }
}

void expectRepeatBoundary(
    const QVector<SubtitleCue> &cues,
    int cueId,
    double mediaDurationSeconds,
    double expectedBoundary,
    const char *scenario)
{
    const double actualBoundary = subtitleCueRepeatBoundary(cues, cueId, mediaDurationSeconds);
    if (std::abs(actualBoundary - expectedBoundary) > 0.0001) {
        QTextStream(stderr) << scenario << ": expected boundary " << expectedBoundary
                            << ", got " << actualBoundary << '\n';
        ++failures;
    }
}
}

int main()
{
    const QVector<SubtitleCue> cues = {
        {0, 10.0, 14.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 20.0, 24.0, QStringLiteral("second"), QStringLiteral("test")},
        {2, 30.0, 34.0, QStringLiteral("third"), QStringLiteral("test")},
    };

    expectCue({}, 12.0, SubtitleNavigationDirection::Previous, -1, "empty cues previous");
    expectCue({}, 12.0, SubtitleNavigationDirection::Next, -1, "empty cues next");
    expectCue(cues, 5.0, SubtitleNavigationDirection::Previous, -1, "before first previous");
    expectCue(cues, 5.0, SubtitleNavigationDirection::Next, 0, "before first next");
    expectCue(cues, 12.0, SubtitleNavigationDirection::Previous, -1, "inside first previous");
    expectCue(cues, 12.0, SubtitleNavigationDirection::Next, 1, "inside first next");
    expectCue(cues, 17.0, SubtitleNavigationDirection::Previous, 0, "gap previous");
    expectCue(cues, 17.0, SubtitleNavigationDirection::Next, 1, "gap next");
    expectCue(cues, 20.0, SubtitleNavigationDirection::Previous, 0, "exact start previous");
    expectCue(cues, 20.0, SubtitleNavigationDirection::Next, 2, "exact start next");
    expectCue(cues, 22.0, SubtitleNavigationDirection::Previous, 0, "inside middle previous");
    expectCue(cues, 22.0, SubtitleNavigationDirection::Next, 2, "inside middle next");
    expectCue(cues, 40.0, SubtitleNavigationDirection::Previous, 2, "after last previous");
    expectCue(cues, 40.0, SubtitleNavigationDirection::Next, -1, "after last next");
    expectCue(cues, std::numeric_limits<double>::quiet_NaN(), SubtitleNavigationDirection::Next, -1, "invalid position");

    const SubtitleCue *cueById = findSubtitleCueById(cues, 1);
    if (!cueById || cueById->id != 1) {
        QTextStream(stderr) << "cue lookup by id should find the selected repeat cue\n";
        ++failures;
    }
    if (findSubtitleCueById(cues, 99)) {
        QTextStream(stderr) << "cue lookup by id should reject unknown ids\n";
        ++failures;
    }
    const SubtitleCue *repeatPrevious = findAdjacentSubtitleCueById(cues, 1, SubtitleNavigationDirection::Previous);
    const SubtitleCue *repeatNext = findAdjacentSubtitleCueById(cues, 1, SubtitleNavigationDirection::Next);
    if (!repeatPrevious || repeatPrevious->id != 0 || !repeatNext || repeatNext->id != 2) {
        QTextStream(stderr) << "repeat navigation should stay anchored to the selected cue\n";
        ++failures;
    }
    if (findAdjacentSubtitleCueById(cues, 0, SubtitleNavigationDirection::Previous)
        || findAdjacentSubtitleCueById(cues, 2, SubtitleNavigationDirection::Next)) {
        QTextStream(stderr) << "repeat navigation should stop at subtitle list boundaries\n";
        ++failures;
    }

    expectRepeatBoundary(cues, 0, 100.0, 16.0, "long gap keeps two seconds of subtitle tail");
    expectRepeatBoundary(cues, 0, 15.0, 15.0, "media duration clips the tail allowance");
    expectRepeatBoundary(cues, 1, 100.0, 26.0, "repeat boundary is capped at two seconds after subtitle end");
    expectRepeatBoundary(cues, 2, 32.0, 32.0, "media duration clips subtitle end");
    expectRepeatBoundary(cues, 2, 100.0, 36.0, "last subtitle keeps two seconds of tail");
    expectRepeatBoundary(cues, 99, 100.0, -1.0, "unknown subtitle boundary");

    const QVector<SubtitleCue> closeSpacedCues = {
        {0, 10.0, 14.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 15.5, 19.0, QStringLiteral("second"), QStringLiteral("test")},
    };
    expectRepeatBoundary(closeSpacedCues, 0, 100.0, 15.5, "short gap uses next subtitle start");

    const QVector<SubtitleCue> overlappingCues = {
        {0, 10.0, 17.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 16.0, 20.0, QStringLiteral("second"), QStringLiteral("test")},
    };
    expectRepeatBoundary(overlappingCues, 0, 100.0, 16.0, "overlap still stops at next subtitle start");

    const QVector<SubtitleCue> openEndedCues = {
        {0, 10.0, 14.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 20.0, -1.0, QStringLiteral("last"), QStringLiteral("test")},
    };
    expectCue(openEndedCues, 30.0, SubtitleNavigationDirection::Previous, 0, "open-ended last previous");
    expectCue(openEndedCues, 30.0, SubtitleNavigationDirection::Next, -1, "open-ended last next");
    expectRepeatBoundary(openEndedCues, 1, 42.0, 42.0, "open-ended last cue uses media duration");

    const QVector<SubtitleCue> missingEndCues = {
        {0, 10.0, -1.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 20.0, 24.0, QStringLiteral("second"), QStringLiteral("test")},
    };
    expectRepeatBoundary(missingEndCues, 0, 40.0, 20.0, "missing end uses next subtitle start");
    expectRepeatBoundary(missingEndCues, 0, 15.0, 15.0, "media duration clips inferred subtitle end");

    return failures == 0 ? 0 : 1;
}
