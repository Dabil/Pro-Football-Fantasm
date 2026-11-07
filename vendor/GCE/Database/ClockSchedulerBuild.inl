// Private clock construction participant in fresh World6. Core snapshot remains read-only.
void instantiateClockScheduler(CreationContext& c,std::int64_t coreSeason,const std::string& utc,const WorldDatabaseCreateRequest& request)
{
    namespace S=TeamDataSql;
    auto* db=c.world;
    if(request.initializationPolicy!=WorldInitializationPolicy::PreparedSeason)
        throw WorldDatabaseException("UNSUPPORTED_EVENT: offseason initialization requires initial-state data and handlers");
    S::run(db,"INSERT INTO WorldSettings VALUES(?,'Clock.InitializationPolicy','PREPARED_SEASON_V1')",{c.worldId});
    S::exec(db,"UPDATE WorldFeatureVersions SET ContractVersion=3 WHERE FeatureKey='VENUE_EVENTS'; UPDATE WorldFeatureVersions SET ContractVersion=2 WHERE FeatureKey IN ('WORLD_SCHEDULER','LEAGUE_WEEKS');");
    for(const auto* feature:{"GAME_CLOCKS","GAME_WINDOWS","ELAPSED_GAME_TIME","TEMPORAL_EVENTS","EVENT_DELIVERY","SEASON_EVENTS"})
        S::run(db,"INSERT INTO WorldFeatureVersions VALUES(?,?,1)",{c.worldId,std::string(feature)});
    for(const auto* kind:{"SEASON_EVENT","TRADE_EVENT","VENUE_CEREMONY"})
        S::run(db,"INSERT INTO WorldEventKinds VALUES(?,?,1,1)",{c.worldId,std::string(kind)});
    S::Query venues(db,"SELECT WorldVenueID,VenueName,TurfType FROM WorldVenues WHERE WorldID=?",{c.worldId});
    while(venues.row()) S::run(db,"INSERT INTO TemporalVenueVersions(WorldID,WorldVenueID,EffectiveUtc,DataRevision,VenueName,TurfType,StateHash) VALUES(?,?,?,0,?,?,?)",
        {c.worldId,venues.integer(),utc,venues.text(1),S::nullable(venues.optionalText(2)),ClockTime::digest(venues.text(1)+":"+venues.optionalText(2).value_or("<NULL>"))});
    S::Query profiles(c.core,"SELECT ProfileKey,ProfileVersion,RegulationPeriods,QuarterSeconds,OvertimePolicyKey,TimingPolicyKey,TimingPolicyVersion,PolicyContentHash FROM CoreGameClockProfiles ORDER BY ProfileKey,ProfileVersion");
    while(profiles.row()) S::run(db,"INSERT INTO GameClockProfiles VALUES(?,?,?,?,?,?,?,?,?)",
        {c.worldId,profiles.text(),profiles.integer(1),profiles.integer(2),profiles.integer(3),profiles.text(4),profiles.text(5),profiles.integer(6),profiles.text(7)});
    for(const auto& team:c.teamSeasonByCoreId)
    {
        S::Query zone(c.core,"SELECT IanaZoneID,TzdbVersion,SourceFileID,SourceLine FROM CoreTeamHomeTimeZones WHERE TeamSeasonID=?",{team.first});
        if(zone.row()) S::run(db,"INSERT INTO TeamHomeTimeZones(WorldID,TeamSeasonID,EffectiveUtc,IanaZoneID,TzdbVersion,SourceHash) VALUES(?,?,?,?,?,?)",
            {c.worldId,team.second,utc,zone.text(),zone.text(1),readCoreMetadata(c.core,"HomeZoneSource."+std::to_string(zone.integer(2))+".Hash")});
    }
    if(request.initializationPolicy!=WorldInitializationPolicy::PreparedSeason)
        throw WorldDatabaseException("UNSUPPORTED_EVENT: offseason maintenance requires initial-state data and handlers, not replay over a prepared Core");
    const auto seasonKey=[&]{S::Query q(c.core,"SELECT p.CompetitionKey||'_'||s.SeasonYear FROM CompetitionSeasons s JOIN Competitions p ON p.CompetitionID=s.CompetitionID WHERE s.CompetitionSeasonID=?",{coreSeason});if(!q.row())throw WorldDatabaseException("Missing source competition season");return q.text();};
    std::map<std::string,std::int64_t> definitions,occurrences;
    std::int64_t trading=-1; std::string tradingAt;
    S::Query events(c.core,"SELECT EventKey,EventType,TriggerKind,LocalDateTime,SourceZone,FixedUtc,Priority,HandlerPolicyKey,HandlerPolicyVersion,SourceLine FROM CoreSeasonEventDefinitions WHERE SeasonKey=? ORDER BY FixedUtc,Priority,EventKey",{seasonKey()});
    while(events.row())
    {
        const auto kind=events.text(2),fixed=events.text(5),policy=events.text(7);
        // A local calendar label alone cannot establish an absolute past instant.
        // Unresolved team-local definitions remain pending/readiness-blocked.
        const bool past=kind=="FIXED"&&fixed<utc;
        const bool adopted=past&&(policy=="OPEN_TRADING"||policy=="CLOSE_TRADING");
        const std::string mode=adopted?"ADOPTED_FROM_CORE":past?"HISTORICAL_REFERENCE":"PENDING";
        S::run(db,"INSERT INTO SeasonEventDefinitions(WorldID,CompetitionSeasonID,EventKey,EventType,TriggerKind,LocalDateTime,SourceZone,FixedUtc,SourceFileHash,SourceLine,Priority,PolicyKey,PolicyVersion,InitializationMode) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {c.worldId,c.competitionSeasonId,events.text(),events.text(1),kind,S::nullable(events.optionalText(3)),kind=="TEAM_HOME_LOCAL"?S::Value("TEAM_HOME_CITY"):S::nullable(events.optionalText(4)),S::nullable(events.optionalText(5)),readCoreMetadata(c.core,"SeasonEvents.SourceFileHash"),events.integer(9),events.integer(6),policy,events.integer(8),mode});
        const auto definition=S::inserted(db);definitions[events.text()]=definition;
        if(adopted&&fixed>=tradingAt){trading=policy=="OPEN_TRADING"?1:0;tradingAt=fixed;}
        auto instance=[&](const std::string& scope,const std::string& at,S::Value team,S::Value binding,const std::string& zone){
            S::run(db,"INSERT INTO ScheduledWorldEvents(WorldID,EventKey,KindKey,StartsUtc,EndMode,CreatedUtc,CreatedRevision,Visibility,State,DispatchPriority) VALUES(?,?,'SEASON_EVENT',?,'INSTANT',?,0,'PUBLIC','DRAFT',?)",
                {c.worldId,"season:"+events.text()+":"+scope,at,utc,events.integer(6)});
            const auto id=S::inserted(db);
            S::run(db,"INSERT INTO SeasonEventInstances(WorldID,ScheduledWorldEventID,DefinitionID,InstanceScopeKey,TeamSeasonID,ZoneBindingID,ResolvedZone,ConversionPolicyVersion) VALUES(?,?,?,?,?,?,?,?)",
                {c.worldId,id,definition,scope,team,binding,zone,std::string(ClockTime::Policy)});
            S::run(db,"UPDATE ScheduledWorldEvents SET State='PENDING' WHERE WorldID=? AND ScheduledWorldEventID=?",{c.worldId,id});
            if(scope=="LEAGUE")occurrences[events.text()]=id;
        };
        if(past||kind=="AFTER_CHAMPIONSHIP_FINAL"||policy=="DISPATCH_MATCHUP_EVENT")continue;
        if(kind=="FIXED")instance("LEAGUE",fixed,nullptr,nullptr,events.text(4));
        else
        {
            for(const auto& pair:c.teamSeasonByCoreId)
            {
                S::Query z(db,"SELECT BindingID,IanaZoneID,TzdbVersion FROM TeamHomeTimeZones WHERE WorldID=? AND TeamSeasonID=? ORDER BY EffectiveUtc DESC LIMIT 1",{c.worldId,pair.second});
                if(!z.row()||z.text(2)!=ClockTime::Policy)continue;
                std::string at;
                try{at=ClockTime::normalize(events.text(3),z.text(1));}
                catch(const std::exception& e){c.result->diagnostics.push_back({DatabaseDiagnosticSeverity::Warning,e.what(),{},0});continue;}
                if(at>=utc)instance("TEAM:"+std::to_string(pair.second),at,pair.second,z.integer(),z.text(1));
                else S::run(db,"INSERT INTO WorldSettings VALUES(?,?,?)",{c.worldId,"Clock.TeamHistoricalReference:"+std::to_string(definition)+":"+std::to_string(pair.second),at});
            }
        }
    }
    if(trading>=0)S::run(db,"INSERT INTO SeasonTradingVersions(WorldID,CompetitionSeasonID,EffectiveUtc,DataRevision,TradingOpen) VALUES(?,?,?,0,?)",{c.worldId,c.competitionSeasonId,utc,trading});
    S::Query deps(c.core,"SELECT EventKey,RequiresEventKey FROM CoreSeasonEventDependencies");
    while(deps.row())
    {
        if(!definitions.count(deps.text())||!definitions.count(deps.text(1)))continue;
        S::run(db,"INSERT INTO WorldSettings VALUES(?,?,'REQUIRES_SUCCESS')",{c.worldId,"Clock.SourceDependency:"+std::to_string(definitions.at(deps.text()))+":"+std::to_string(definitions.at(deps.text(1)))});
        if(occurrences.count(deps.text())&&occurrences.count(deps.text(1)))
            S::run(db,"INSERT INTO EventDependencies VALUES(?,?,?)",{c.worldId,occurrences.at(deps.text()),occurrences.at(deps.text(1))});
    }
    if(S::scalar(db,"SELECT count(*) FROM GameClockProfiles WHERE WorldID=?",{c.worldId})==0)
        c.result->diagnostics.push_back({DatabaseDiagnosticSeverity::Warning,"MISSING_CLOCK_PROFILE: configure league ground rules before game execution.",{},0});
    if(S::scalar(db,"SELECT count(*) FROM SeasonEventDefinitions WHERE WorldID=? AND TriggerKind='TEAM_HOME_LOCAL' AND InitializationMode='PENDING'",{c.worldId})>0 && S::scalar(db,"SELECT count(*) FROM TeamHomeTimeZones WHERE WorldID=?",{c.worldId})<static_cast<std::int64_t>(c.teamSeasonByCoreId.size()))
        c.result->diagnostics.push_back({DatabaseDiagnosticSeverity::Warning,"MISSING_TIMEZONE: team-local templates remain unresolved until authored home-city zones are installed; no timezone was inferred.",{},0});
}
