#pragma once
#include "GCE/Database/TeamDataSql.h"
#include "GCE/Database/ClockTime.h"
#include <limits>

// Private persistence participant. Public callers use trusted WorldActor services.
namespace ClockStateSql
{
namespace S=TeamDataSql;
inline bool enabled(sqlite3* db)
{return S::scalar(db,"SELECT count(*) FROM WorldSchemaInfo WHERE SchemaVersion=5 AND SchemaFamily='GCE_WORLD'")==1;}
inline bool contracts(sqlite3* db,std::int64_t world)
{
    return S::scalar(db,"SELECT count(*) FROM WorldFeatureVersions WHERE WorldID=? AND ((FeatureKey='WORLD_SCHEDULER' AND ContractVersion=2) OR (FeatureKey='VENUE_EVENTS' AND ContractVersion=3) OR (FeatureKey='LEAGUE_WEEKS' AND ContractVersion=2) OR (FeatureKey IN ('GAME_CLOCKS','GAME_WINDOWS','ELAPSED_GAME_TIME','TEMPORAL_EVENTS','EVENT_DELIVERY','SEASON_EVENTS') AND ContractVersion=1))",{world})==9 &&
           S::scalar(db,"SELECT count(*) FROM WorldTimingPolicies WHERE WorldID=? AND PolicyKey='VENUE_SEVEN_DAYS_V1' AND PolicyVersion=1 AND DurationSeconds=604800",{world})==1;
}
inline bool publication(sqlite3* db,std::int64_t world)
{
    if(S::scalar(db,"SELECT count(*) FROM WorldExecutionContexts c JOIN WorldDataRevisions r ON r.WorldID=c.WorldID WHERE c.WorldID=? AND c.DataRevision>r.Revision",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM SeasonEventInstances i JOIN SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID JOIN ScheduledWorldEvents e ON e.WorldID=i.WorldID AND e.ScheduledWorldEventID=i.ScheduledWorldEventID WHERE i.WorldID=? AND ((d.TriggerKind='FIXED' AND d.FixedUtc<>e.StartsUtc) OR d.Priority<>e.DispatchPriority OR (i.TeamSeasonID IS NOT NULL AND NOT EXISTS(SELECT 1 FROM TeamSeasons t WHERE t.TeamSeasonID=i.TeamSeasonID AND t.CompetitionSeasonID=d.CompetitionSeasonID)) OR (i.ZoneBindingID IS NOT NULL AND NOT EXISTS(SELECT 1 FROM TeamHomeTimeZones z WHERE z.WorldID=i.WorldID AND z.BindingID=i.ZoneBindingID AND z.TeamSeasonID=i.TeamSeasonID)))",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM WorldVenues v WHERE v.WorldID=? AND NOT EXISTS(SELECT 1 FROM TemporalVenueVersions t WHERE t.WorldID=v.WorldID AND t.WorldVenueID=v.WorldVenueID AND t.ContextID IS NULL)",{world})) return false;
    if(S::scalar(db,"SELECT count(*) FROM ScheduledWorldEvents e WHERE e.WorldID=? AND e.KindKey IN ('SEASON_EVENT','TRADE_EVENT','VENUE_CEREMONY') AND NOT EXISTS(SELECT 1 FROM SeasonEventInstances i JOIN SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE i.WorldID=e.WorldID AND i.ScheduledWorldEventID=e.ScheduledWorldEventID AND d.InitializationMode='PENDING')",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM EventOutcomes o JOIN ScheduledWorldEvents e ON e.WorldID=o.WorldID AND e.ScheduledWorldEventID=o.ScheduledWorldEventID WHERE o.WorldID=? AND (e.State<>'COMPLETED' OR e.ResolvedUtc<>o.HistoricalUtc OR NOT EXISTS(SELECT 1 FROM SchedulerDispatchReceipts r WHERE r.WorldID=o.WorldID AND r.ScheduledWorldEventID=o.ScheduledWorldEventID AND r.ContextID=o.ContextID))",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM EventOutcomes o JOIN SeasonEventInstances i ON i.WorldID=o.WorldID AND i.ScheduledWorldEventID=o.ScheduledWorldEventID JOIN SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE o.WorldID=? AND d.PolicyKey IN ('OPEN_TRADING','CLOSE_TRADING') AND NOT EXISTS(SELECT 1 FROM TemporalEffectReceipts r JOIN SeasonTradingVersions v ON v.WorldID=r.WorldID AND v.VersionID=r.TradingVersionID WHERE r.WorldID=o.WorldID AND r.OutcomeID=o.OutcomeID AND r.EffectKey=d.PolicyKey AND v.OutcomeID=o.OutcomeID AND v.CompetitionSeasonID=d.CompetitionSeasonID AND v.TradingOpen=(d.PolicyKey='OPEN_TRADING'))",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM VenueChangeEvents e WHERE e.WorldID=? AND e.State='APPLIED' AND NOT EXISTS(SELECT 1 FROM EventOutcomes o JOIN TemporalEffectReceipts r ON r.WorldID=o.WorldID AND r.OutcomeID=o.OutcomeID JOIN TemporalVenueVersions v ON v.WorldID=r.WorldID AND v.VersionID=r.VenueVersionID WHERE o.WorldID=e.WorldID AND o.ScheduledWorldEventID=e.ScheduledWorldEventID AND o.ContextID=e.ResolutionContextID AND v.ContextID=o.ContextID AND ((e.FieldCode='NAME' AND v.VenueName IS e.DesiredValue) OR (e.FieldCode='SURFACE' AND v.TurfType IS e.DesiredValue)))",{world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM GameExecutionClaims c WHERE c.WorldID=? AND c.State='COMMITTED' AND NOT EXISTS(SELECT 1 FROM GameExecutionCheckpoints p WHERE p.WorldID=c.WorldID AND p.ClaimID=c.ClaimID)",{world}))return false;
    if(S::scalar(db,"WITH RECURSIVE reach(a,b) AS (SELECT ScheduledWorldEventID,RequiresEventID FROM EventDependencies WHERE WorldID=? UNION SELECT r.a,d.RequiresEventID FROM reach r JOIN EventDependencies d ON d.ScheduledWorldEventID=r.b WHERE d.WorldID=?) SELECT count(*) FROM reach WHERE a=b",{world,world}))return false;
    if(S::scalar(db,"SELECT count(*) FROM CausalReadFences f WHERE f.WorldID=? AND f.ResourceKind='VENUE' AND NOT EXISTS(SELECT 1 FROM TemporalVenueVersions v WHERE v.WorldID=f.WorldID AND CAST(v.WorldVenueID AS TEXT)=f.ResourceKey AND v.StateHash=f.VersionHash AND (v.EffectiveUtc<f.AsOfUtc OR (v.EffectiveUtc=f.AsOfUtc AND v.EffectiveOrder<=f.AsOfOrder)))",{world}))return false;
    return true;
}
struct Trading {bool known=false,open=false;std::string deadline;};
inline Trading tradingAt(sqlite3* db,std::int64_t world,std::int64_t season,const std::string& at,std::int64_t order=std::numeric_limits<std::int64_t>::max())
{
    Trading r;
    S::Query q(db,"SELECT TradingOpen FROM SeasonTradingVersions WHERE WorldID=? AND CompetitionSeasonID=? AND (EffectiveUtc<? OR (EffectiveUtc=? AND EffectiveOrder<=?)) ORDER BY EffectiveUtc DESC,EffectiveOrder DESC,DataRevision DESC,VersionID DESC LIMIT 1",{world,season,at,at,order});
    if(q.row()){r.known=true;r.open=q.integer()!=0;}
    S::Query d(db,"SELECT min(FixedUtc) FROM SeasonEventDefinitions WHERE WorldID=? AND CompetitionSeasonID=? AND PolicyKey='CLOSE_TRADING' AND TriggerKind='FIXED'",{world,season});
    if(d.row())r.deadline=d.text();if(!r.deadline.empty()&&at>=r.deadline){r.known=true;r.open=false;}return r;
}
inline std::int64_t editContext(sqlite3* db,std::int64_t world,std::int64_t user,const std::string& now,std::int64_t revision,bool god)
{
    const auto order=S::scalar(db,"SELECT COALESCE(max(OrderInInstant),0)+1 FROM WorldExecutionContexts WHERE WorldID=? AND EffectiveUtc=?",{world,now});
    S::run(db,"INSERT INTO WorldExecutionContexts(WorldID,ActorUserID,DriverKind,EffectiveUtc,WorldAnchorUtc,DataRevision,PhaseCode,CommandKey,OrderInInstant) VALUES(?,?,?,?,?,?,1,?,?)",
           {world,user,std::string(god?"ADMIN":"EDIT"),now,now,revision,"edit:"+std::to_string(revision)+":"+std::to_string(user),order});
    return S::inserted(db);
}
inline bool fenced(sqlite3* db,std::int64_t world,const std::string& kind,const std::string& key,const std::string& at,std::int64_t order,std::int64_t own=0)
{
    return S::scalar(db,"SELECT count(*) FROM CausalReadFences WHERE WorldID=? AND ResourceKind=? AND ResourceKey=? AND ContextID<>? AND (AsOfUtc>? OR (AsOfUtc=? AND AsOfOrder>=?))",{world,kind,key,own,at,at,order})>0;
}
struct Venue {std::int64_t id=0;std::string name,hash;std::optional<std::string> surface;};
inline Venue venueAt(sqlite3* db,std::int64_t world,std::int64_t venue,const std::string& at,std::int64_t order=std::numeric_limits<std::int64_t>::max())
{
    S::Query q(db,"SELECT VersionID,VenueName,TurfType,StateHash FROM TemporalVenueVersions WHERE WorldID=? AND WorldVenueID=? AND (EffectiveUtc<? OR (EffectiveUtc=? AND EffectiveOrder<=?)) ORDER BY EffectiveUtc DESC,EffectiveOrder DESC,DataRevision DESC,VersionID DESC LIMIT 1",{world,venue,at,at,order});
    if(!q.row())throw std::runtime_error("INTEGRITY_FAILURE: venue has no eligible historical version");
    return {q.integer(),q.text(1),q.text(3),q.optionalText(2)};
}
inline void directVenue(sqlite3* db,std::int64_t world,std::int64_t venue,const std::string& at,std::int64_t revision,std::int64_t context,const std::string& name,const std::optional<std::string>& surface)
{
    const auto order=S::scalar(db,"SELECT OrderInInstant FROM WorldExecutionContexts WHERE WorldID=? AND ContextID=?",{world,context});
    S::run(db,"INSERT INTO TemporalVenueVersions(WorldID,WorldVenueID,EffectiveUtc,DataRevision,VenueName,TurfType,ContextID,StateHash,EffectiveOrder) VALUES(?,?,?,?,?,?,?,?,?)",
           {world,venue,at,revision,name,S::nullable(surface),context,ClockTime::digest(std::to_string(name.size())+":"+name+(surface?":SET:"+*surface:":NULL")),order});
}
// Enlisted typed participants: only a trusted dispatcher may create the owning
// outcome/context and finalize its fact/receipt in the SAME write transaction.
inline void requireOutcomeTransaction(sqlite3* db)
{
    if(sqlite3_get_autocommit(db)||sqlite3_txn_state(db,"main")!=SQLITE_TXN_WRITE)
        throw std::runtime_error("Temporal effect requires its dispatcher's active write transaction");
}
inline bool prerequisitesReady(sqlite3* db,std::int64_t world,std::int64_t occurrence)
{
    // Preserve definition-level dependencies even when no dated parent occurrence
    // exists. A reference/adoption is not a forged successful execution receipt.
    return S::scalar(db,"SELECT count(*) FROM SeasonEventInstances child JOIN SeasonEventDefinitions parent ON parent.WorldID=child.WorldID JOIN WorldSettings s ON s.WorldID=child.WorldID AND s.SettingKey='Clock.SourceDependency:'||child.DefinitionID||':'||parent.DefinitionID WHERE child.WorldID=? AND child.ScheduledWorldEventID=? AND (NOT EXISTS(SELECT 1 FROM SeasonEventInstances p WHERE p.WorldID=parent.WorldID AND p.DefinitionID=parent.DefinitionID) OR EXISTS(SELECT 1 FROM SeasonEventInstances p JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID AND e.ScheduledWorldEventID=p.ScheduledWorldEventID WHERE p.WorldID=parent.WorldID AND p.DefinitionID=parent.DefinitionID AND e.State<>'COMPLETED'))",{world,occurrence})==0;
}
inline std::int64_t applyTradingOutcome(sqlite3* db,std::int64_t world,std::int64_t outcome)
{
    requireOutcomeTransaction(db);
    S::Query q(db,"SELECT d.CompetitionSeasonID,d.PolicyKey,o.HistoricalUtc,c.DataRevision,c.OrderInInstant FROM EventOutcomes o JOIN WorldExecutionContexts c ON c.WorldID=o.WorldID AND c.ContextID=o.ContextID JOIN SeasonEventInstances i ON i.WorldID=o.WorldID AND i.ScheduledWorldEventID=o.ScheduledWorldEventID JOIN SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE o.WorldID=? AND o.OutcomeID=? AND d.PolicyKey IN ('OPEN_TRADING','CLOSE_TRADING') AND o.HandlerKey=d.PolicyKey AND o.HandlerVersion=d.PolicyVersion",{world,outcome});
    if(!q.row())throw std::runtime_error("Trading outcome has no supported typed season policy");
    const auto occurrence=S::scalar(db,"SELECT ScheduledWorldEventID FROM EventOutcomes WHERE WorldID=? AND OutcomeID=?",{world,outcome});
    if(!prerequisitesReady(db,world,occurrence))throw std::runtime_error("UNSATISFIED_DEPENDENCY: typed trading work requires successful prerequisite execution");
    const auto key=q.text(1);
    S::Query replay(db,"SELECT TradingVersionID FROM TemporalEffectReceipts WHERE WorldID=? AND OutcomeID=? AND EffectKey=?",{world,outcome,key});
    if(replay.row())return replay.integer();
    S::run(db,"INSERT INTO SeasonTradingVersions(WorldID,CompetitionSeasonID,EffectiveUtc,DataRevision,TradingOpen,OutcomeID,EffectiveOrder) VALUES(?,?,?,?,?,?,?)",{world,q.integer(),q.text(2),q.integer(3),std::int64_t(key=="OPEN_TRADING"),outcome,q.integer(4)});
    const auto id=S::inserted(db);S::run(db,"INSERT INTO TemporalEffectReceipts(WorldID,OutcomeID,EffectKey,TradingVersionID) VALUES(?,?,?,?)",{world,outcome,key,id});return id;
}
inline std::int64_t applyVenueOutcome(sqlite3* db,std::int64_t world,std::int64_t outcome)
{
    requireOutcomeTransaction(db);
    S::Query q(db,"SELECT v.VenueChangeEventID,v.WorldVenueID,v.FieldCode,v.DesiredValue,o.HistoricalUtc,c.DataRevision,c.OrderInInstant,c.ContextID,v.ScheduledWorldEventID FROM EventOutcomes o JOIN WorldExecutionContexts c ON c.WorldID=o.WorldID AND c.ContextID=o.ContextID JOIN VenueChangeEvents v ON v.WorldID=o.WorldID AND v.ScheduledWorldEventID=o.ScheduledWorldEventID JOIN VenueChangePlans p ON p.WorldID=v.WorldID AND p.PlanID=v.PlanID WHERE o.WorldID=? AND o.OutcomeID=? AND p.DueUtc=o.HistoricalUtc AND o.HandlerKey='VENUE_APPLY' AND o.HandlerVersion=1",{world,outcome});
    if(!q.row())throw std::runtime_error("Venue outcome has no supported due plan");
    const auto field=q.text(2);
    S::Query replay(db,"SELECT VenueVersionID FROM TemporalEffectReceipts WHERE WorldID=? AND OutcomeID=? AND EffectKey=?",{world,outcome,field});
    if(replay.row())return replay.integer();
    if(q.integer(6)<1)throw std::runtime_error("Dispatcher context must reserve an order after the baseline");
    auto current=venueAt(db,world,q.integer(1),q.text(4),q.integer(6)-1);
    S::run(db,"INSERT INTO CausalReadFences(WorldID,ContextID,ResourceKind,ResourceKey,AsOfUtc,VersionHash,AsOfOrder) VALUES(?,?,'VENUE',?,?,?,?)",
        {world,q.integer(7),std::to_string(q.integer(1)),q.text(4),current.hash,q.integer(6)-1});
    const auto before=field=="NAME"?std::optional<std::string>(current.name):current.surface;
    if(field=="NAME")current.name=q.text(3);else current.surface=q.optionalText(3);
    S::run(db,"INSERT INTO TemporalVenueVersions(WorldID,WorldVenueID,EffectiveUtc,DataRevision,VenueName,TurfType,SourceEventID,ContextID,StateHash,EffectiveOrder) VALUES(?,?,?,?,?,?,?,?,?,?)",
        {world,q.integer(1),q.text(4),q.integer(5),current.name,S::nullable(current.surface),q.integer(8),q.integer(7),ClockTime::digest(std::to_string(current.name.size())+":"+current.name+(current.surface?":SET:"+*current.surface:":NULL")),q.integer(6)});
    const auto id=S::inserted(db);S::run(db,"INSERT INTO TemporalEffectReceipts(WorldID,OutcomeID,EffectKey,VenueVersionID) VALUES(?,?,?,?)",{world,outcome,field,id});
    S::run(db,"UPDATE VenueChangeEvents SET State='APPLIED',ResolvedUtc=?,ResolvedRevision=?,ResolutionReason='DUE_TIME',AppliedBeforeValue=?,ResolutionContextID=? WHERE WorldID=? AND VenueChangeEventID=? AND State='PENDING'",
        {q.text(4),q.integer(5),S::nullable(before),q.integer(7),world,q.integer()});
    if(sqlite3_changes(db)!=1)throw std::runtime_error("Venue request changed during dispatch");return id;
}
}
