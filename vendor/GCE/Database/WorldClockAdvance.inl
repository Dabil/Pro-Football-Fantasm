// CLOCK-SCHEDULER-TASK1-2026-11-04. Private World5 World-clock driver, included
// inside the trusted scheduler namespace. Game-local execution is a later task.
bool clockDependenciesReady(Session& s, std::int64_t occurrence)
{
    return ClockStateSql::prerequisitesReady(s.db,s.world,occurrence) &&
        S::scalar(s.db,"SELECT count(*) FROM EventDependencies d JOIN ScheduledWorldEvents p "
            "ON p.WorldID=d.WorldID AND p.ScheduledWorldEventID=d.RequiresEventID "
            "WHERE d.WorldID=? AND d.ScheduledWorldEventID=? "
            "AND (p.State<>'COMPLETED' OR p.ResolutionReason='SUPPORT_PENDING')",
            {s.world,occurrence})==0;
}
std::int64_t clockAttention(Session& s,std::int64_t occurrence)
{
    S::Query q(s.db,"SELECT p.ParticipationID,p.WorldUserID,p.ScheduledWorldEventID,e.KindKey "
        "FROM WorldEventParticipants p JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID "
        "AND e.ScheduledWorldEventID=p.ScheduledWorldEventID JOIN Characters c ON c.WorldID=p.WorldID "
        "AND c.CharacterID=p.CharacterID AND c.PersonID=p.PersonID AND c.WorldUserID=p.WorldUserID "
        "JOIN WorldUsers u ON u.WorldID=p.WorldID AND u.WorldUserID=p.WorldUserID "
        "WHERE p.WorldID=? AND e.ScheduledWorldEventID=? AND p.Required=1 AND p.State='OFFERED' AND e.State='PENDING' "
        "AND e.StartsUtc<=? AND p.OfferedStartUtc<=? AND c.StartsUtc<=? AND c.EndsUtc IS NULL AND u.Active=1 "
        "ORDER BY e.StartsUtc,e.DispatchPriority,e.ScheduledWorldEventID,p.ParticipationID",
        {s.world,occurrence,s.now,s.now,s.now});
    while(q.row())
        if(q.text(3)!="GAME_KICKOFF" || controlsGameOffer(s,q.integer(),q.integer(1)))
            return q.integer(2);
    return 0;
}
void clockDispatch(Session& s,std::int64_t request,std::int64_t event,
                   const std::string& handler,std::int64_t rev,
                   std::int64_t game=0,std::int64_t claim=0,const std::string& historical={})
{
    const auto at=historical.empty()?s.now:historical;
    const bool skipped=handler=="SUPPORT_PENDING";
    const auto sequence=S::scalar(s.db,"SELECT COALESCE(max(OrderInInstant),0)+1 "
        "FROM WorldExecutionContexts WHERE WorldID=? AND EffectiveUtc=?",{s.world,at});
    if(game)
        S::run(s.db,"INSERT INTO WorldExecutionContexts(WorldID,ActorUserID,DriverKind,GameID,ClaimID,EffectiveUtc,WorldAnchorUtc,DataRevision,PhaseCode,CommandKey,OrderInInstant) VALUES(?,?,'GAME',?,?,?,?,?,1,?,?)",
            {s.world,s.user,game,claim,at,s.now,rev,"game-dispatch:"+std::to_string(event),sequence});
    else
    S::run(s.db,"INSERT INTO WorldExecutionContexts(WorldID,ActorUserID,DriverKind,AdvanceRequestID,"
        "EffectiveUtc,WorldAnchorUtc,DataRevision,PhaseCode,CommandKey,OrderInInstant) "
        "VALUES(?,?,'WORLD',?,?,?,?,1,?,?)",
        {s.world,s.user,request,at,s.now,rev,"world-dispatch:"+std::to_string(event),sequence});
    const auto context=S::inserted(s.db);
    const auto fingerprint=ClockTime::digest(handler+":"+std::to_string(event)+":"+at);
    S::run(s.db,"INSERT INTO EventOutcomes(WorldID,ScheduledWorldEventID,ContextID,HistoricalUtc,"
        "HandlerKey,HandlerVersion,OutcomeHash,PolicyKey) VALUES(?,?,?,?,?,1,?,?)",
        {s.world,event,context,at,handler,fingerprint,handler});
    const auto outcome=S::inserted(s.db);
    std::int64_t venue=0;
    if(handler=="VENUE_APPLY")
    {
        ClockStateSql::applyVenueOutcome(s.db,s.world,outcome);
        venue=S::scalar(s.db,"SELECT VenueChangeEventID FROM VenueChangeEvents WHERE WorldID=? "
            "AND ScheduledWorldEventID=?",{s.world,event});
    }
    else if(handler=="OPEN_TRADING" || handler=="CLOSE_TRADING")
        ClockStateSql::applyTradingOutcome(s.db,s.world,outcome);
    S::run(s.db,"UPDATE ScheduledWorldEvents SET State='COMPLETED',StartedUtc=StartsUtc,ResolvedUtc=?,"
        "ResolvedRevision=?,ResolutionReason=? WHERE WorldID=? "
        "AND ScheduledWorldEventID=? AND State='PENDING'",
        {at,rev,std::string(skipped?"SUPPORT_PENDING":"HISTORICAL_DISPATCH"),s.world,event});
    require(sqlite3_changes(s.db)==1,WorldEditError::IntegrityFailure,"Event changed during dispatch");
    S::run(s.db,"INSERT INTO SchedulerDispatchReceipts VALUES(?,?,?,?,?,?,?)",
        {s.world,event,std::string(skipped?"RESOLVE":"APPLY"),context,fingerprint,at,rev});
    const std::string kind=handler=="VENUE_APPLY"?"VENUE_APPLIED":handler;
    S::run(s.db,"INSERT INTO WorldEventFacts(WorldID,FactKey,FactKind,ScheduledWorldEventID,"
        "VenueChangeEventID,OccurredUtc,DataRevision,Visibility,ExecutionContextID) "
        "SELECT ?,?,?,?,?,?,?,Visibility,? FROM ScheduledWorldEvents "
        "WHERE WorldID=? AND ScheduledWorldEventID=?",
        {s.world,"world-dispatch:"+std::to_string(event),kind,event,
         venue?S::Value(venue):S::Value(nullptr),at,rev,context,s.world,event});
    const auto fact=S::inserted(s.db);
    S::run(s.db,"INSERT INTO WorldFactReaders SELECT WorldID,?,WorldUserID,"
        "CASE WHEN KnownFromUtc>? THEN KnownFromUtc ELSE ? END FROM WorldEventReaders "
        "WHERE WorldID=? AND ScheduledWorldEventID=?",{fact,at,at,s.world,event});
}
WorldAdvanceOutcome clockAdvanceUnit(Session& s,S::Transaction& tx,std::int64_t id,std::int64_t rev)
{
    const auto finish=[&](const std::string& state,const std::string& reason,std::int64_t event=0,int phase=0){
        advanceState(s,id,state,reason,event,phase,rev);
        schedulerRevision(s,rev);
        auto r=advanceOutcome(s,id);r.units=1;tx.commit();return r;
    };
    // Even a fully computed game must not unlock World movement until its window
    // has a durable RESOLVED receipt. Cancellation of an intent cannot unlock it.
    if(S::scalar(s.db,"SELECT count(*) FROM GameProcessingWindows WHERE WorldID=? "
        "AND State IN ('ADMITTED','PAUSED')",{s.world}))
        return finish("BLOCKED","WINDOW_IN_PROGRESS");
    if(S::scalar(s.db,"SELECT count(*) FROM GameProcessingWindows WHERE WorldID=? AND State='DRAFT'",{s.world}))
        return finish("BLOCKED","INTEGRITY_FAILURE");
    if(S::scalar(s.db,"SELECT count(*) FROM ScheduledWorldEvents WHERE WorldID=? AND State='RUNNING'",{s.world}) ||
       S::scalar(s.db,"SELECT count(*) FROM Games WHERE WorldID=? AND GameStatus='IN_PROGRESS'",{s.world}))
        return finish("BLOCKED","CLOCK_BARRIER");
    // Undated local templates cannot safely be stepped over: their missing zone
    // prevents establishing their position in the global queue.
    if(!S::scalar(s.db,"SELECT count(*) FROM WorldSettings WHERE WorldID=? AND SettingKey=? AND SettingValue='GAME_TIME_REDUCED_V1'",{s.world,"Admission.Scope:"+std::to_string(id)}) && S::scalar(s.db,"SELECT count(*) FROM SeasonEventDefinitions d WHERE d.WorldID=? "
        "AND d.InitializationMode='PENDING' AND d.TriggerKind='TEAM_HOME_LOCAL' AND "
        "((SELECT count(*) FROM SeasonEventInstances i WHERE i.WorldID=d.WorldID AND i.DefinitionID=d.DefinitionID)+"
        "(SELECT count(*) FROM WorldSettings x WHERE x.WorldID=d.WorldID AND "
        "x.SettingKey LIKE 'Clock.TeamHistoricalReference:'||d.DefinitionID||':%'))<"
        "(SELECT count(*) FROM TeamSeasons t WHERE t.CompetitionSeasonID=d.CompetitionSeasonID AND t.Status='ACTIVE')",
        {s.world})) return finish("BLOCKED","MISSING_TIMEZONE");
    S::Query intent(s.db,"SELECT Mode,TargetUtc,VisibleStopEventID,StopReason,PauseEventID FROM WorldAdvanceRequests "
        "WHERE WorldID=? AND AdvanceRequestID=?",{s.world,id});
    require(intent.row(),WorldEditError::InvalidValue,"Unknown World advancement request");
    std::string target=intent.text(1);
    if(intent.text()=="NEXT_EVENT")
    {
        S::Query anchor(s.db,"SELECT CASE WHEN State='COMPLETED' AND KindKey='GAME_KICKOFF' "
            "THEN ResolvedUtc ELSE StartsUtc END,State FROM ScheduledWorldEvents WHERE WorldID=? "
            "AND ScheduledWorldEventID=?",{s.world,intent.integer(2)});
        require(anchor.row(),WorldEditError::IntegrityFailure,"Missing next-event anchor");
        if(anchor.text(1)=="CANCELED" || anchor.text(1)=="SUPERSEDED")
            return finish("COMPLETED","TARGET_CHANGED");
        target=anchor.text();
    }
    S::Query overdue(s.db,"SELECT ScheduledWorldEventID FROM ScheduledWorldEvents WHERE WorldID=? "
        "AND State='PENDING' AND StartsUtc<? ORDER BY StartsUtc,ScheduledWorldEventID LIMIT 1",{s.world,s.now});
    if(overdue.row())return finish("BLOCKED","CLOCK_BARRIER",overdue.integer());
    // Scan all automatic work at this instant so a ready prerequisite can run
    // before a nominally higher-priority dependent. Never skip to a later date.
    S::Query due(s.db,"SELECT e.ScheduledWorldEventID,e.KindKey,k.HandlerVersion,"
        "COALESCE(d.PolicyKey,''),COALESCE(d.PolicyVersion,0) FROM ScheduledWorldEvents e "
        "JOIN WorldEventKinds k ON k.WorldID=e.WorldID AND k.KindKey=e.KindKey "
        "LEFT JOIN SeasonEventInstances i ON i.WorldID=e.WorldID AND i.ScheduledWorldEventID=e.ScheduledWorldEventID "
        "LEFT JOIN SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID "
        "WHERE e.WorldID=? AND e.State='PENDING' AND e.StartsUtc=? AND e.KindKey<>'GAME_KICKOFF' "
        "ORDER BY e.DispatchPriority,k.PhaseCode,e.ScheduledWorldEventID",{s.world,s.now});
    std::int64_t blocked=0;
    while(due.row())
    {
        const auto event=due.integer();
        if(!clockDependenciesReady(s,event)){if(!blocked)blocked=event;continue;}
        if(clockAttention(s,event))return finish("PAUSED","REQUIRED_ATTENTION",event,1);
        const auto handler=due.text(1)=="VENUE_APPLY"?std::string("VENUE_APPLY"):due.text(3);
        const bool unsupported=due.integer(2)!=1 || (handler!="VENUE_APPLY" && due.integer(4)!=1) ||
            (handler!="VENUE_APPLY" && handler!="OPEN_TRADING" && handler!="CLOSE_TRADING" &&
             handler!="ANNOUNCE_LEAGUE_SEASON_ENDED");
        if(unsupported && (intent.text(3)!="UNSUPPORTED_EVENT" || intent.integer(4)!=event))
            return finish("BLOCKED","UNSUPPORTED_EVENT",event,1);
        if(!unsupported && handler=="ANNOUNCE_LEAGUE_SEASON_ENDED" && !S::scalar(s.db,
            "SELECT count(*) FROM SeasonEventInstances i JOIN GameFinalSaves f ON f.WorldID=i.WorldID "
            "AND f.GameID=i.ParentGameID WHERE i.WorldID=? AND i.ScheduledWorldEventID=? AND f.CompletedUtc=?",
            {s.world,event,s.now}))return finish("BLOCKED","UNSATISFIED_DEPENDENCY",event,1);
        advanceState(s,id,"RUNNING","",0,1,rev); // Historical context binds a live intent.
        clockDispatch(s,id,event,unsupported?"SUPPORT_PENDING":handler,rev);
        return finish("RUNNING","",0,1);
    }
    if(blocked)return finish("BLOCKED","UNSATISFIED_DEPENDENCY",blocked,1);
    S::Query kickoff(s.db,"SELECT ScheduledWorldEventID FROM ScheduledWorldEvents WHERE WorldID=? "
        "AND State='PENDING' AND StartsUtc=? AND KindKey='GAME_KICKOFF' ORDER BY ScheduledWorldEventID LIMIT 1",
        {s.world,s.now});
    if(kickoff.row())
    {
        if(clockAttention(s,kickoff.integer()))return finish("PAUSED","REQUIRED_ATTENTION",kickoff.integer(),2);
        return finish("BLOCKED","GAME_WINDOW_ADMISSION_REQUIRED",kickoff.integer(),3);
    }
    if(target<=s.now)return finish("COMPLETED","TARGET_REACHED",0,3);
    S::Query boundary(s.db,"SELECT min(StartsUtc) FROM ScheduledWorldEvents WHERE WorldID=? "
        "AND State='PENDING' AND StartsUtc>?",{s.world,s.now});boundary.row();
    const auto next=boundary.text();const auto to=next.empty()||next>target?target:next;
    require(S::utc(to)&&to>s.now,WorldEditError::ClockBarrier,"Invalid next World boundary");
    S::run(s.db,"UPDATE Worlds SET CurrentTimeUtc=? WHERE WorldID=?",{to,s.world});s.now=to;
    return finish("RUNNING","");
}
