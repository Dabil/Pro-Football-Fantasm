#pragma once
#include "GCE/Database/WorldTeamData.h"

// WORLD-EVENT-TIME-STAGE2-2026-11-04. Trusted services; no SQL/provider ownership
// passes to the UI. Cache read models by World revision/time, never hover/Tab.
enum class WorldAdvanceMode
{
    NextEvent,
    ToTime
};
struct WorldAdvanceOutcome
{
    // revision/time are current; committedRevision is the original intent receipt,
    // outcomeRevision is this request's latest persisted state.
    std::int64_t requestId = 0, revision = 0, committedRevision = 0, outcomeRevision = 0, stopEventId = 0, windowId = 0;
    std::string state, reason, currentTimeUtc;
    bool replayed = false, stale = false;
    std::size_t units = 0;
};
struct WorldTimelineEntry
{
    std::int64_t eventId = 0;
    std::string kind, startsUtc, endsUtc, endMode, state;
};
struct WorldTimeline
{
    std::int64_t revision = 0;
    std::string currentTimeUtc;
    bool canAdvance = false;
    std::vector<WorldTimelineEntry> events;
};
struct WorldEventFact
{
    std::int64_t factId = 0, eventId = 0, venueRequestId = 0, gameId = 0;
    std::string kind, occurredUtc;
};
enum class WorldEventChoiceKind
{
    Attend,
    Decline,
    Delegate
};
struct WorldEventChoice
{
    std::int64_t participationId = 0;
    WorldEventChoiceKind choice = WorldEventChoiceKind::Attend;
};
struct WorldEventChoiceOffer
{
    std::int64_t participationId = 0, eventId = 0, personId = 0;
    std::string startsUtc, endsUtc, endMode, state;
    bool required = false;
};
WorldEditResult<WorldTimeline> loadWorldTimeline(const WorldActor&, const std::string& fromUtc,
                                                 const std::string& toUtc, std::size_t offset = 0,
                                                 std::size_t limit = 100);
WorldEditResult<std::vector<WorldEventFact>> loadWorldFacts(const WorldActor&, const std::string& fromUtc,
                                                            const std::string& toUtc, std::size_t offset = 0,
                                                            std::size_t limit = 100);
WorldEditResult<std::vector<WorldEventChoiceOffer>> loadWorldEventChoices(const WorldActor&);
WorldEditResult<WorldAdvanceOutcome> loadWorldAdvance(const WorldActor&, std::int64_t requestId);
WorldEditResult<WorldAdvanceOutcome> beginWorldAdvance(const WorldActor&, const std::string& commandKey,
                                                       WorldAdvanceMode,
                                                       const std::optional<std::string>& targetUtc,
                                                       std::int64_t expectedRevision);
WorldEditResult<WorldAdvanceOutcome> continueWorldAdvance(const WorldActor&, std::int64_t requestId,
                                                          std::int64_t expectedRevision,
                                                          std::size_t budget = 64);
WorldEditResult<WorldAdvanceOutcome> cancelWorldAdvance(const WorldActor&, std::int64_t requestId,
                                                        std::int64_t expectedRevision);
WorldEditResult<WorldEditCommit> submitEventChoices(const WorldActor&, const std::string& commandKey,
                                                    const std::vector<WorldEventChoice>&,
                                                    std::int64_t expectedRevision);

// Exact kickoff cohorts. One admitted window resolves before another is admitted;
// recorded historical finish times never change the next scheduled kickoff.
struct WorldGameWindow
{
    std::int64_t worldId = 0, revision = 0, games = 0, windowId = 0;
    std::string worldKey, currentTimeUtc, kickoffUtc, reason;
    bool canStart = false, canResume = false;
    std::optional<WorldAdvanceOutcome> advance;
};
WorldEditResult<WorldGameWindow> loadNextGameWindow(const WorldActor&, std::int64_t competitionSeasonId);
WorldEditResult<WorldAdvanceOutcome> beginNextGameWindow(const WorldActor&, std::int64_t competitionSeasonId,
    const std::string& commandKey, std::int64_t expectedRevision, const std::string& providerInputs = {});
