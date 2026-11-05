// Included after the trusted actor/session helpers, not a public SQL entry point.
WorldEditResult<WorldTradingState> loadWorldTradingState(const WorldActor& actor,std::int64_t season)
{
    return attempt<WorldTradingState>([&]{
        Session s(actor,false);
        require(s.clockModel,WorldEditError::UnsupportedFeature,"Trading temporal state requires World5");
        require(S::scalar(s.db,"SELECT count(*) FROM CompetitionSeasons cs JOIN WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND cs.CompetitionSeasonID=?",{s.world,season})==1,WorldEditError::InvalidValue,"Season is outside this World");
        WorldTradingState r;r.competitionSeasonId=season;r.revision=s.revision;r.asOfUtc=s.now;
        const auto state=ClockStateSql::tradingAt(s.db,s.world,season,s.now);
        r.known=state.known;r.open=state.open;r.deadlineUtc=state.deadline;
        S::exec(s.db,"COMMIT");return r;
    });
}
WorldEditResult<WorldClockReadiness> loadClockSchedulerReadiness(const WorldActor& actor,std::int64_t season)
{
    return attempt<WorldClockReadiness>([&]{
        Session s(actor,false);
        require(S::scalar(s.db,"SELECT count(*) FROM CompetitionSeasons cs JOIN WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND cs.CompetitionSeasonID=? AND cs.Status='ACTIVE' AND c.Active=1",{s.world,season})==1,WorldEditError::InvalidValue,"No active season in this World");
        WorldClockReadiness r;r.worldId=s.world;r.competitionSeasonId=season;r.revision=s.revision;
        if(!s.clockModel)r.blockers.push_back("UNSUPPORTED_SCHEMA: World4 remains on its original version path");
        else
        {
            if(!S::scalar(s.db,"SELECT count(*) FROM GameClockProfiles WHERE WorldID=?",{s.world}))r.blockers.push_back("MISSING_CLOCK_PROFILE: configure regulation quarter duration and policies");
            S::Query missing(s.db,"SELECT d.EventKey FROM SeasonEventDefinitions d WHERE d.WorldID=? AND d.CompetitionSeasonID=? AND d.InitializationMode='PENDING' AND d.TriggerKind='TEAM_HOME_LOCAL' AND ((SELECT count(*) FROM SeasonEventInstances i WHERE i.WorldID=d.WorldID AND i.DefinitionID=d.DefinitionID)+(SELECT count(*) FROM WorldSettings s WHERE s.WorldID=d.WorldID AND s.SettingKey LIKE 'Clock.TeamHistoricalReference:'||d.DefinitionID||':%')) < (SELECT count(*) FROM TeamSeasons t WHERE t.CompetitionSeasonID=d.CompetitionSeasonID AND t.Status='ACTIVE')",{s.world,season});
            while(missing.row())r.blockers.push_back("MISSING_TIMEZONE: incomplete team-local expansion for "+missing.text());
            S::Query handlers(s.db,"SELECT EventKey,PolicyKey FROM SeasonEventDefinitions WHERE WorldID=? AND CompetitionSeasonID=? AND InitializationMode='PENDING' AND PolicyKey NOT IN ('OPEN_TRADING','CLOSE_TRADING','ANNOUNCE_LEAGUE_SEASON_ENDED')",{s.world,season});
            while(handlers.row())r.blockers.push_back("UNSUPPORTED_EVENT: "+handlers.text()+" requires "+handlers.text(1));
            S::Query dependencies(s.db,"SELECT d.EventKey FROM SeasonEventDefinitions d JOIN SeasonEventDefinitions p ON p.WorldID=d.WorldID JOIN WorldSettings s ON s.WorldID=d.WorldID AND s.SettingKey='Clock.SourceDependency:'||d.DefinitionID||':'||p.DefinitionID WHERE d.WorldID=? AND d.CompetitionSeasonID=? AND d.InitializationMode='PENDING' AND p.InitializationMode='HISTORICAL_REFERENCE'",{s.world,season});
            while(dependencies.row())r.blockers.push_back("UNSATISFIED_DEPENDENCY: "+dependencies.text()+" requires successful work; a historical reference is not a completion receipt");
            r.blockers.push_back("FULL_PFF_PROVIDER_REQUIRED: admission and the reduced GameTime executor are installed; full football/checkpoint capabilities remain incomplete");
        }
        r.ready=r.blockers.empty();S::exec(s.db,"COMMIT");return r;
    });
}
