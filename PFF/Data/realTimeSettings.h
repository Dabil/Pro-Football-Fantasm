#pragma once

// First-pass simulation estimates, in elapsed seconds unless noted otherwise.
// These measure time in the game's history, not computer execution time or the
// football countdown. GameTime timing integration will consume these settings.
namespace PffRealTimeSettings
{
    // From the snap until the play ends; excludes preparation for the next snap.
    inline constexpr int RunningPlaySeconds = 6;
    inline constexpr int CompletedPassSeconds = 8;
    inline constexpr int IncompletePassSeconds = 6;
    inline constexpr int PuntSeconds = 8;
    inline constexpr int FieldGoalAttemptSeconds = 8;

    // From the end of a play until the next snap.
    inline constexpr int BetweenPlaysClockRunningSeconds = 34;
    inline constexpr int BetweenPlaysClockStoppedSeconds = 40;

    // Stoppages and period transitions.
    inline constexpr int CalledTimeoutSeconds = 60;
    inline constexpr int QuarterBreakSeconds = 120; // After quarters one and three.
    inline constexpr int HalftimeSeconds = 720;
    inline constexpr int CommercialBreakSeconds = 120;

    // Commercials may use eligible possession changes or scoring breaks.
    // This is a cap, not a requirement to manufacture four breaks per quarter.
    inline constexpr int MaxCommercialBreaksPerQuarter = 4;

    // Overlapping stoppages share elapsed time: use the longest applicable
    // duration instead of adding timeout, commercial and period-break durations.
    inline constexpr bool OverlappingStoppagesUseLongestDuration = true;
}
