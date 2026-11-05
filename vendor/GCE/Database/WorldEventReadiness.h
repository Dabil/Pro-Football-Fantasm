#pragma once
#include "GCE/Database/TeamDataSql.h"
#include "GCE/Database/ClockStateSql.h"

// WORLD-EVENT-TIME-STAGE1-2026-11-03. Cross-row publication checks supplement
// schema guards. Call inside the builder/writer transaction and load snapshot.
inline bool worldEventContractsReady(sqlite3 *db, std::int64_t world)
{
    if(ClockStateSql::enabled(db)) return ClockStateSql::contracts(db,world);
    namespace S = TeamDataSql;
    return S::scalar(db,
                     "SELECT count(*) FROM WorldFeatureVersions WHERE WorldID=? AND "
                     "((FeatureKey='VENUE_EVENTS' AND ContractVersion=2) OR "
                     "(FeatureKey IN ('WORLD_SCHEDULER','LEAGUE_WEEKS') AND ContractVersion=1))",
                     {world}) == 3 &&
           S::scalar(db,
                     "SELECT count(*) FROM WorldEventKinds WHERE WorldID=? AND HandlerVersion=1 AND "
                     "((KindKey='GAME_KICKOFF' AND PhaseCode=3) OR (KindKey='VENUE_APPLY' AND PhaseCode=1))",
                     {world}) == 2 &&
           S::scalar(db,
                     "SELECT count(*) FROM WorldTimingPolicies WHERE WorldID=? AND "
                     "PolicyKey='VENUE_SEVEN_DAYS_V1' AND PolicyVersion=1 AND DurationSeconds=604800",
                     {world}) == 1;
}

inline bool worldEventPublicationReady(sqlite3 *db, std::int64_t world)
{
    namespace S = TeamDataSql;
    if(ClockStateSql::enabled(db) && !ClockStateSql::publication(db,world)) return false;
    if (S::scalar(db,
                  "SELECT count(*) FROM VenueChangePlans p WHERE p.WorldID=? AND NOT EXISTS "
                  "(SELECT 1 FROM VenueChangeEvents v WHERE v.WorldID=p.WorldID AND v.PlanID=p.PlanID)",
                  {world}) != 0 ||
        S::scalar(db,
                  "SELECT count(*) FROM VenueChangeCommands c WHERE c.WorldID=? AND NOT EXISTS "
                  "(SELECT 1 FROM VenueChangeEvents v WHERE v.WorldID=c.WorldID AND "
                  "(v.CommandID=c.CommandID OR v.ResolutionCommandID=c.CommandID))",
                  {world}) != 0)
        return false;
    // No incomplete drafts, detached occurrences or incomplete lifecycle pairs.
    if (S::scalar(db, R"SQL(
SELECT count(*) FROM ScheduledWorldEvents e WHERE e.WorldID=? AND
 (e.State='DRAFT' OR
 (e.KindKey='GAME_KICKOFF' AND NOT EXISTS(SELECT 1 FROM GameKickoffOccurrences k
   WHERE k.WorldID=e.WorldID AND k.ScheduledWorldEventID=e.ScheduledWorldEventID)) OR
 (e.KindKey='VENUE_APPLY' AND NOT EXISTS(SELECT 1 FROM VenueChangeEvents v WHERE
   v.WorldID=e.WorldID AND v.ScheduledWorldEventID=e.ScheduledWorldEventID AND
   ((v.State='PENDING' AND e.State='PENDING') OR (v.State='APPLIED' AND e.State='COMPLETED') OR
    (v.State=e.State AND v.State IN ('CANCELED','SUPERSEDED'))))))
)SQL",
                  {world}) != 0)
        return false;
    if (S::scalar(db, R"SQL(
SELECT count(*) FROM WorldScheduledGames g JOIN CompetitionSeasons cs ON cs.CompetitionSeasonID=g.CompetitionSeasonID
JOIN WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND
 (SELECT count(*) FROM GameKickoffOccurrences k JOIN ScheduledWorldEvents e ON
 e.WorldID=k.WorldID AND e.ScheduledWorldEventID=k.ScheduledWorldEventID WHERE k.WorldID=c.WorldID
 AND k.WorldScheduledGameID=g.WorldScheduledGameID AND k.IsCurrent=1 AND e.StartsUtc=g.ScheduledUtc
 AND e.State IN ('PENDING','RUNNING','COMPLETED'))<>1
)SQL",
                  {world}) != 0)
        return false;
    // Each request has one announcement/revision fact. God cancellation deliberately
    // has no new fact, while user cancellation and actual application do.
    return S::scalar(db, R"SQL(
SELECT count(*) FROM VenueChangeEvents v WHERE v.WorldID=? AND
 ((SELECT count(*) FROM WorldEventFacts f WHERE f.WorldID=v.WorldID AND f.VenueChangeEventID=v.VenueChangeEventID
   AND f.ScheduledWorldEventID=v.ScheduledWorldEventID AND f.FactKind=CASE WHEN v.SupersedesEventID IS NULL
   THEN 'VENUE_ANNOUNCED' ELSE 'VENUE_REVISED' END AND f.OccurredUtc=v.AnnouncedUtc AND f.DataRevision=v.AnnouncedRevision)<>1
 OR (v.State='CANCELED' AND v.ResolutionReason='USER_CANCEL' AND
   (SELECT count(*) FROM WorldEventFacts f WHERE f.WorldID=v.WorldID AND f.VenueChangeEventID=v.VenueChangeEventID
    AND f.FactKind='VENUE_CANCELED' AND f.OccurredUtc=v.ResolvedUtc AND f.DataRevision=v.ResolvedRevision)<>1)
 OR (v.State='APPLIED' AND ((SELECT count(*) FROM SchedulerDispatchReceipts r WHERE r.WorldID=v.WorldID
    AND r.ScheduledWorldEventID=v.ScheduledWorldEventID AND r.StepKey='APPLY' AND r.OccurredUtc=v.ResolvedUtc)<>1
   OR (SELECT count(*) FROM WorldEventFacts f WHERE f.WorldID=v.WorldID AND f.VenueChangeEventID=v.VenueChangeEventID
    AND f.FactKind='VENUE_APPLIED' AND f.OccurredUtc=v.ResolvedUtc AND f.DataRevision=v.ResolvedRevision)<>1)))
)SQL",
                     {world}) == 0;
}
