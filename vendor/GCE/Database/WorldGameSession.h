#pragma once
#include "GCE/Database/WorldTeamData.h"
#include "GCE/Database/WorldGameSessionState.h"
#include <functional>
#include <optional>
#include <string_view>

// GAMETIME-TEST-2026-11-04: bounded development harness, not FULL_PFF readiness.
// Owns queue admission and atomic buffered-game publication. No football rules.
struct WorldTestQueuedEvent
{
    int elapsedSeconds = 0;
    std::string notice, blocker, reason, startsUtc;
};
struct WorldTestMatchup
{
    std::int64_t gameId = 0, fixtureId = 0, windowId = 0, awayId = 0, homeId = 0;
    std::string awayName, homeName, kickoffUtc;
    std::filesystem::path pbpPath;
    std::string awayCoreKey, homeCoreKey;
    std::uint32_t seed = 0;
    std::string inputHash;
    std::optional<WorldTestSavedState> saved;
    std::vector<WorldTestQueuedEvent> queuedEvents;
    std::optional<WorldTestQueuedEvent> interruption;
    int pauseThrough = -1;
    WorldEditCommit commit;
};
struct WorldTestGameStep
{
    std::int64_t generation = 0, playOrdinal = 0;
    int period = 1, remaining = 900, elapsed = 0, awayScore = 0, homeScore = 0;
    bool final = false, play = true;
    std::string text, teamStatistics;
};
WorldEditResult<WorldTestMatchup> beginTestMatchup(const WorldActor &, std::int64_t season,
                                                   std::int64_t expectedRevision,
                                                   const std::string &inputEvidence);
WorldEditResult<WorldTestMatchup> loadFinalTestMatchup(const WorldActor &, std::int64_t game);
WorldEditResult<WorldEditCommit> finalizeTestMatchup(const WorldActor &, std::int64_t game,
                                                     const std::vector<WorldTestGameStep> &,
                                                     std::int64_t expectedRevision,
                                                     std::string *recordedText = nullptr,
                                                     const WorldTestSavedState *state = nullptr,
                                                     const std::function<std::string(std::string_view)> &taggedOutput = {});
WorldEditResult<WorldEditCommit> saveTestMatchup(const WorldActor &, std::int64_t game,
                                               const std::vector<WorldTestGameStep> &,
                                               const WorldTestSavedState &,
                                               std::int64_t expectedRevision, int pauseThrough = -1);
