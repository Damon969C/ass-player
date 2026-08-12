#include "subtitle_navigation.h"

#include <QTextStream>

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

    const QVector<SubtitleCue> openEndedCues = {
        {0, 10.0, 14.0, QStringLiteral("first"), QStringLiteral("test")},
        {1, 20.0, -1.0, QStringLiteral("last"), QStringLiteral("test")},
    };
    expectCue(openEndedCues, 30.0, SubtitleNavigationDirection::Previous, 0, "open-ended last previous");
    expectCue(openEndedCues, 30.0, SubtitleNavigationDirection::Next, -1, "open-ended last next");

    return failures == 0 ? 0 : 1;
}
