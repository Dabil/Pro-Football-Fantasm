// Private helpers included in WorldTeamData.cpp's trusted service namespace.
WorldLeagueWeekProgress weekProgress(Session& s, std::int64_t season)
{
    require(
        S::scalar(s.db,
                  "SELECT count(*) FROM CompetitionSeasons cs JOIN WorldCompetitions c ON "
                  "c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND cs.CompetitionSeasonID=?",
                  {s.world, season}) == 1,
        WorldEditError::InvalidValue, "Competition season is outside this World");
    WorldLeagueWeekProgress r;
    r.competitionSeasonId = season;
    r.revision = s.revision;
    std::string weekSql =
               "SELECT w.LeagueWeekID,w.WeekKey,n.LeagueWeekID,n.WeekKey,n.State,w.SequenceNo,n.SequenceNo "
               "FROM LeagueWeeks w LEFT JOIN LeagueWeeks n ON n.WorldID=w.WorldID AND "
               "n.CompetitionSeasonID=w.CompetitionSeasonID AND n.LeagueWeekID=w.NextLeagueWeekID WHERE "
               "w.WorldID=? AND w.CompetitionSeasonID=? AND w.State='OPEN'";
    if(s.clockModel) {
        const std::string horizon="(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=w.WorldID)";
        auto at=weekSql.find("w.State='OPEN'");
        weekSql.replace(at,std::string("w.State='OPEN'").size(),"w.OpenedUtc<="+horizon+" AND (w.ClosedUtc IS NULL OR w.ClosedUtc>"+horizon+")");
        at=weekSql.find("n.State,");
        weekSql.replace(at,8,"CASE WHEN n.OpenedUtc IS NULL OR n.OpenedUtc>"+horizon+" THEN 'FUTURE' WHEN n.ClosedUtc<="+horizon+" THEN 'CLOSED' ELSE 'OPEN' END,");
    }
    S::Query q(s.db,weekSql.c_str(),{s.world,season});
    if (!q.row())
    {
        r.awaitingSuccessor = S::scalar(s.db,
                                        "SELECT count(*) FROM LeagueWeeks WHERE WorldID=? AND "
                                        "CompetitionSeasonID=? AND State='CLOSED' AND ClosedUtc<=(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=LeagueWeeks.WorldID)",
                                        {s.world, season}) > 0;
        return r;
    }
    r.currentWeekId = q.integer();
    r.currentWeekKey = q.text(1);
    if (!q.null(2))
    {
        require(q.text(4) == "FUTURE" && q.integer(6) == q.integer(5) + 1, WorldEditError::IntegrityFailure,
                "Invalid supported week successor");
        r.nextWeekId = q.integer(2);
        r.nextWeekKey = q.text(3);
    }
    require(!q.row(), WorldEditError::IntegrityFailure, "Ambiguous current league week");
    r.requiredGames =
        S::scalar(s.db, "SELECT count(*) FROM LeagueWeekGames WHERE WorldID=? AND LeagueWeekID=?",
                  {s.world, r.currentWeekId});
    r.finalizedGames = S::scalar(
        s.db,
        "SELECT count(*) FROM LeagueWeekGames m JOIN FinalizedLeagueGameResults f ON f.WorldID=m.WorldID AND "
        "f.WorldScheduledGameID=m.WorldScheduledGameID WHERE m.WorldID=? AND m.LeagueWeekID=? AND f.CompletedUtc<=(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=m.WorldID)",
        {s.world, r.currentWeekId});
    r.ready = r.requiredGames > 0;
    return r;
}
std::vector<WorldVenueEvent> venueEvents(Session& s, std::int64_t venue, std::size_t offset,
                                         std::size_t limit, bool pendingOnly = false)
{
    require(limit > 0 && limit <= 500 && offset <= 1000000, WorldEditError::InvalidValue,
            "Invalid event page");
    require(S::scalar(s.db, "SELECT count(*) FROM WorldVenues WHERE WorldID=? AND WorldVenueID=?",
                      {s.world, venue}) == 1,
            WorldEditError::InvalidValue, "Venue is outside this World");
    std::string sql =
        "SELECT e.VenueChangeEventID,e.CommandID,e.WorldVenueID,e.FieldCode,e.State,e.AnnouncedUtc,"
        "e.ResolvedUtc,e.ResolutionReason,e.DesiredValue,e.AppliedBeforeValue,e.SupersedesEventID,"
        "e.ResolutionCommandID,e.AnnouncedRevision,e.ResolvedRevision,c.WorldUserID,c.WorldOrganizationID,"
        "e.PlanID,e.ScheduledWorldEventID,p.OriginalAnnouncedUtc,p.DueUtc,p.TimingPolicyKey,"
        "p.TimingPolicyVersion,p.DurationSeconds FROM VenueChangeEvents e JOIN VenueChangeCommands c "
        "ON c.WorldID=e.WorldID AND c.CommandID=e.CommandID JOIN VenueChangePlans p "
        "ON p.WorldID=e.WorldID AND p.PlanID=e.PlanID WHERE e.WorldID=? AND e.WorldVenueID=? ";
    if(s.clockModel) {
        sql += "AND e.AnnouncedUtc<=(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=e.WorldID) ";
        const std::string h="(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=e.WorldID)";
        auto replaceAll=[&](const std::string& a,const std::string& b){std::size_t i=0;while((i=sql.find(a,i))!=std::string::npos){sql.replace(i,a.size(),b);i+=b.size();}};
        replaceAll("e.State,","CASE WHEN e.ResolvedUtc>"+h+" THEN 'PENDING' ELSE e.State END,");
        for(const auto* col:{"ResolvedUtc","ResolutionReason","AppliedBeforeValue","ResolutionCommandID","ResolvedRevision"}) {
            const std::string a="e."+std::string(col)+",";
            replaceAll(a,"CASE WHEN e.ResolvedUtc<="+h+" THEN e."+col+" END,");
        }
    }
    if (pendingOnly) sql += s.clockModel?"AND (e.State='PENDING' OR e.ResolvedUtc>(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=e.WorldID)) ":"AND e.State='PENDING' ";
    sql += "ORDER BY e.VenueChangeEventID LIMIT ? OFFSET ?";
    S::Query q(s.db, sql.c_str(),
               {s.world, venue, static_cast<std::int64_t>(limit), static_cast<std::int64_t>(offset)});
    std::vector<WorldVenueEvent> out;
    while (q.row())
    {
        WorldVenueEvent e;
        e.eventId = q.integer();
        e.commandId = q.integer(1);
        e.venueId = q.integer(2);
        e.field = q.text(3);
        e.state = q.text(4);
        e.announcedUtc = q.text(5);
        e.resolvedUtc = q.text(6);
        e.reason = q.text(7);
        e.desiredValue = q.optionalText(8);
        e.appliedBeforeValue = q.optionalText(9);
        e.supersedesEventId = q.integer(10);
        e.resolutionCommandId = q.integer(11);
        e.announcedRevision = q.integer(12);
        e.resolvedRevision = q.integer(13);
        e.requestedByUserId = q.integer(14);
        e.organizationId = q.integer(15);
        e.planId = q.integer(16);
        e.scheduledEventId = q.integer(17);
        e.originalAnnouncedUtc = q.text(18);
        e.dueUtc = q.text(19);
        e.timingPolicyKey = q.text(20);
        e.timingPolicyVersion = q.integer(21);
        e.durationSeconds = q.integer(22);
        out.push_back(std::move(e));
    }
    return out;
}
// Versioned, length-prefixed canonical request encoding, not a security hash.
// It retains explicit absent/set/clear distinctions and omits revision for retries.
void fingerprintPart(std::string &out, const std::string &value)
{
    out += std::to_string(value.size()) + ":" + value;
}
void fingerprintOptional(std::string &out, const std::optional<std::string> &value)
{
    fingerprintPart(out, value ? "SET" : "ABSENT");
    if (value)
        fingerprintPart(out, *value);
}
std::string saveFingerprint(const WorldTeamDataDraft &d)
{
    std::string out = "venue-save-v2:";
    for (auto id : {d.worldId, d.organizationId, d.teamSeasonId})
        fingerprintPart(out, std::to_string(id));
    fingerprintOptional(out, d.city ? std::optional<std::string>(text(*d.city, 128, true)) : std::nullopt);
    fingerprintOptional(out,
                        d.nickname ? std::optional<std::string>(text(*d.nickname, 128, true)) : std::nullopt);
    fingerprintOptional(out, d.venueName ? std::optional<std::string>(text(*d.venueName, 256, true))
                                         : std::nullopt);
    fingerprintPart(out, d.surface ? "PATCH" : "ABSENT");
    if (d.surface)
        fingerprintOptional(out, optionalText(*d.surface, 128));
    for (const auto& id : {d.expectedVenueNameEventId, d.expectedVenueSurfaceEventId})
        fingerprintPart(out, id ? std::to_string(*id) : "ABSENT");
    fingerprintPart(out, d.managed ? (*d.managed ? "ON" : "OFF") : "ABSENT");
    auto staff = d.staff;
    std::sort(staff.begin(), staff.end(),
              [](const auto& a, const auto& b) { return a.positionId < b.positionId; });
    for (const auto& a : staff)
    {
        for (auto id : {a.positionId, a.expectedAssignmentId, static_cast<std::int64_t>(a.kind)})
            fingerprintPart(out, std::to_string(id));
        fingerprintPart(out, a.personId ? std::to_string(*a.personId) : "ABSENT");
        fingerprintPart(out, a.newPerson ? "NEW" : "ABSENT");
        if (a.newPerson)
        {
            fingerprintPart(out, text(a.newPerson->displayName, 256, true));
            fingerprintOptional(out, optionalText(a.newPerson->firstName, 128));
            fingerprintOptional(out, optionalText(a.newPerson->lastName, 128));
        }
    }
    return out;
}
std::optional<WorldEditCommit> replayVenueCommand(Session& s, const WorldActor& actor, const std::string& key,
                                                  const std::string& fingerprint)
{
    if (key.empty())
        return {};
    const auto normalized = text(key, 128, true);
    S::Query q(
        s.db,
        "SELECT WorldUserID,RequestFingerprint,WorldOrganizationID,WorldVenueID,DataRevision,OccurredUtc "
        "FROM VenueChangeCommands WHERE WorldID=? AND CommandKey=?",
        {s.world, normalized});
    if (!q.row())
        return {};
    require(q.integer() == s.user && q.text(1) == fingerprint, WorldEditError::IdempotencyConflict,
            "Command key was already used for different content or actor");
    WorldEditCommit r;
    r.worldId = s.world;
    r.worldKey = WorldEditingAccess::worldKey(actor);
    r.changed = true;
    r.replayed = true;
    r.organizations.push_back(q.integer(2));
    r.venues.push_back(q.integer(3));
    r.committedRevision = q.integer(4);
    r.revision = s.revision;
    r.currentTimeUtc = s.now;
    S::Query teams(s.db, "SELECT TeamSeasonID FROM TeamSeasons WHERE WorldOrganizationID=?", {q.integer(2)});
    while (teams.row())
        r.teamSeasons.push_back(teams.integer());
    // Replaying a combined venue/team/staff command must invalidate its people too.
    S::Query people(
        s.db,
        "SELECT DISTINCT a.PersonID FROM RoleAssignments a JOIN RolePositions p ON p.WorldID=a.WorldID AND "
        "p.PositionID=a.PositionID WHERE a.WorldID=? AND p.WorldOrganizationID=? AND a.StartsRevision<=? AND "
        "(a.EndsRevision IS NULL OR a.EndsRevision>=?)",
        {s.world, q.integer(2), r.committedRevision, r.committedRevision});
    while (people.row())
        r.people.push_back(people.integer());
    return r;
}
std::int64_t venueCommand(Session& s, const Team& t, const std::string& key, const std::string& fingerprint,
                          std::int64_t revision)
{
    S::run(s.db,
           "INSERT INTO "
           "VenueChangeCommands(WorldID,CommandKey,RequestFingerprint,WorldUserID,WorldOrganizationID,"
           "TeamSeasonID,WorldVenueID,OccurredUtc,DataRevision) VALUES(?,?,?,?,?,?,?,?,?)",
           {s.world, text(key, 128, true), fingerprint, s.user, t.org, t.id, t.venue, s.now, revision});
    return S::inserted(s.db);
}
// Domain and occurrence resolutions are inseparable, even for technical God cancellation.
void resolveVenueRequest(Session &s, const WorldVenueEvent &event, const std::string &state,
                         const std::string &reason, std::int64_t command, std::int64_t rev)
{
    if(s.clockModel)
    {
        require(!ClockStateSql::fenced(s.db,s.world,"EVENT_QUEUE","WORLD",event.dueUtc,0,s.context),WorldEditError::CausalFence,"CAUSAL_FENCE: pending work has already been consumed by resolved history");
        require(S::scalar(s.db,"SELECT count(*) FROM VenueChangeEvents WHERE WorldID=? AND VenueChangeEventID=? AND State='PENDING'",{s.world,event.eventId})==1,WorldEditError::CausalFence,"CAUSAL_FENCE: this venue change already resolved in recorded history");
    }
    S::run(s.db,
           "UPDATE ScheduledWorldEvents SET State=?,ResolvedUtc=?,ResolvedRevision=?,ResolutionReason=? "
           "WHERE WorldID=? AND ScheduledWorldEventID=? AND State='PENDING'",
           {state, s.now, rev, reason, s.world, event.scheduledEventId});
    require(sqlite3_changes(s.db) == 1, WorldEditError::StalePendingEvent, "Pending occurrence changed");
    if(s.clockModel)
        S::run(s.db,"UPDATE VenueChangeEvents SET State=?,ResolvedUtc=?,ResolvedRevision=?,ResolutionReason=?,ResolutionCommandID=?,ResolutionContextID=? WHERE WorldID=? AND VenueChangeEventID=? AND State='PENDING'",
            {state,s.now,rev,reason,command?S::Value(command):S::Value(nullptr),s.context,s.world,event.eventId});
    else S::run(
        s.db,
        "UPDATE VenueChangeEvents SET State=?,ResolvedUtc=?,ResolvedRevision=?,ResolutionReason=?,"
        "ResolutionCommandID=? WHERE WorldID=? AND VenueChangeEventID=? AND State='PENDING'",
        {state, s.now, rev, reason, command ? S::Value(command) : S::Value(nullptr), s.world, event.eventId});
    require(sqlite3_changes(s.db) == 1, WorldEditError::StalePendingEvent, "Pending venue request changed");
}
void venueFact(Session &s, std::int64_t event, std::int64_t occurrence, const std::string &kind,
               std::int64_t rev)
{
    if(s.clockModel)
    {
        S::run(s.db,"INSERT INTO WorldEventFacts(WorldID,FactKey,FactKind,ScheduledWorldEventID,VenueChangeEventID,OccurredUtc,DataRevision,Visibility,ExecutionContextID) VALUES(?,?,?,?,?,?,?,'PUBLIC',?)",
            {s.world,"venue:"+std::to_string(event)+":"+kind,kind,occurrence,event,s.now,rev,s.context});
        return;
    }
    S::run(s.db,
           "INSERT INTO WorldEventFacts(WorldID,FactKey,FactKind,ScheduledWorldEventID,VenueChangeEventID,"
           "OccurredUtc,DataRevision,Visibility) VALUES(?,?,?,?,?,?,?,'PUBLIC')",
           {s.world, "venue:" + std::to_string(event) + ":" + kind, kind, occurrence, event, s.now, rev});
}
void saveVenue(Session &s, const Team &t, const WorldTeamDataDraft &d, const std::string &fingerprint,
               std::int64_t rev, WorldEditCommit &result)
{
    if (!d.venueName && !d.surface)
        return;
    require(t.venue != 0, WorldEditError::UnsupportedFeature, "This team has no editable venue");
    const auto pending = venueEvents(s, t.venue, 0, 500, true);
    struct Patch
    {
        std::string field;
        std::optional<std::string> desired, current;
        const WorldVenueEvent *event = nullptr;
        bool change = false;
    };
    std::vector<Patch> patches;
    const auto add = [&](const std::string &field, const std::optional<std::string> &desired,
                         const std::optional<std::string> &current,
                         const std::optional<std::int64_t> &expected) {
        const auto p =
            std::find_if(pending.begin(), pending.end(), [&](const auto &e) { return e.field == field; });
        const auto *event = p == pending.end() ? nullptr : &*p;
        if (!s.god)
            require(expected && *expected == (event ? event->eventId : 0), WorldEditError::StalePendingEvent,
                    "Reload venue pending values before editing this field");
        const bool change =
            s.god ? desired != current : (event ? desired != event->desiredValue : desired != current);
        if (change && event && !s.god)
            require(s.now < event->dueUtc, WorldEditError::DueWorkPending,
                    "Due venue work must be processed by the scheduler before changing this request");
        patches.push_back({field, desired, current, event, change});
    };
    if (d.venueName)
        add("NAME", text(*d.venueName, 256, true), t.venueName, d.expectedVenueNameEventId);
    if (d.surface)
        add("SURFACE", optionalText(*d.surface, 128), t.surface, d.expectedVenueSurfaceEventId);
    if (std::none_of(patches.begin(), patches.end(), [](const auto &p) { return p.change; }))
        return;
    require(capability(s, t, "VENUE_EDIT"), WorldEditError::NotAuthorized,
            "Venue editing requires controlled owner authority or God mode");
    require(s.god || shared(s, t) == 0, WorldEditError::SharedVenueReadOnly,
            "Shared venue name and surface are read-only outside God mode");
    std::int64_t command = 0;
    if (!s.god)
    {
        require(!d.venueCommandKey.empty(), WorldEditError::InvalidValue,
                "Managed venue requests require a command key");
        command = venueCommand(s, t, d.venueCommandKey, fingerprint, rev);
    }
    for (const auto &p : patches)
    {
        if (!p.change)
            continue;
        if(s.clockModel && s.god)
            require(!ClockStateSql::fenced(s.db,s.world,"VENUE",std::to_string(t.venue),s.now,s.contextOrder,s.context),WorldEditError::CausalFence,"CAUSAL_FENCE: venue edit would change already resolved history");
        const bool cancel = s.god || p.desired == p.current;
        if (p.event)
        {
            resolveVenueRequest(s, *p.event, cancel ? "CANCELED" : "SUPERSEDED",
                                s.god    ? "GOD_OVERRIDE"
                                : cancel ? "USER_CANCEL"
                                         : "REPLACED",
                                command, rev);
            if (cancel && !s.god)
                venueFact(s, p.event->eventId, p.event->scheduledEventId, "VENUE_CANCELED", rev);
        }
        if (s.god)
        {
            if(!s.clockModel) S::run(s.db,
                   p.field == "NAME" ? "UPDATE WorldVenues SET VenueName=? WHERE WorldID=? AND WorldVenueID=?"
                                     : "UPDATE WorldVenues SET TurfType=? WHERE WorldID=? AND WorldVenueID=?",
                   {S::nullable(p.desired), s.world, t.venue});
        }
        else if (!cancel)
        {
            std::int64_t plan = p.event ? p.event->planId : 0;
            std::string due = p.event ? p.event->dueUtc : std::string{};
            if (!plan)
            {
                S::Query time(s.db,
                              "SELECT strftime('%Y-%m-%dT%H:%M:%SZ',?, '+' || DurationSeconds || ' seconds') "
                              "FROM WorldTimingPolicies WHERE WorldID=? AND PolicyKey='VENUE_SEVEN_DAYS_V1' "
                              "AND PolicyVersion=1 AND DurationSeconds=604800",
                              {s.now, s.world});
                require(time.row() && S::utc(time.text()), WorldEditError::InvalidValue,
                        "Venue deadline is outside supported UTC dates");
                due = time.text();
                S::run(s.db,
                       "INSERT INTO VenueChangePlans(WorldID,WorldVenueID,FieldCode,InitialCommandID,"
                       "OriginalAnnouncedUtc,DueUtc,TimingPolicyKey,TimingPolicyVersion,DurationSeconds) "
                       "VALUES(?,?,?,?,?,?,'VENUE_SEVEN_DAYS_V1',1,604800)",
                       {s.world, t.venue, p.field, command, s.now, due});
                plan = S::inserted(s.db);
            }
            if(s.clockModel)
                require(!ClockStateSql::fenced(s.db,s.world,"EVENT_QUEUE","WORLD",due,0,s.context),WorldEditError::CausalFence,"CAUSAL_FENCE: new venue work would invalidate consumed queue history");
            if(s.clockModel)
                S::run(s.db,"INSERT INTO ScheduledWorldEvents(WorldID,EventKey,KindKey,StartsUtc,EndMode,CreatedUtc,CreatedRevision,Visibility,State,SupersedesEventID,CreationContextID) VALUES(?,?,'VENUE_APPLY',?,'INSTANT',?,?,'PUBLIC','DRAFT',?,?)",
                    {s.world,"venue:"+std::to_string(command)+":"+p.field,due,s.now,rev,p.event?S::Value(p.event->scheduledEventId):S::Value(nullptr),s.context});
            else S::run(s.db,
                   "INSERT INTO ScheduledWorldEvents(WorldID,EventKey,KindKey,StartsUtc,EndMode,CreatedUtc,"
                   "CreatedRevision,Visibility,State,SupersedesEventID) "
                   "VALUES(?,?,'VENUE_APPLY',?,'INSTANT',?,?,'PUBLIC','DRAFT',?)",
                   {s.world, "venue:" + std::to_string(command) + ":" + p.field, due, s.now, rev,
                    p.event ? S::Value(p.event->scheduledEventId) : S::Value(nullptr)});
            const auto occurrence = S::inserted(s.db);
            S::run(
                s.db,
                "INSERT INTO VenueChangeEvents(WorldID,WorldVenueID,FieldCode,PlanID,ScheduledWorldEventID,"
                "DesiredValue,CommandID,SupersedesEventID,AnnouncedUtc,AnnouncedRevision,State) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,'PENDING')",
                {s.world, t.venue, p.field, plan, occurrence, S::nullable(p.desired), command,
                 p.event ? S::Value(p.event->eventId) : S::Value(nullptr), s.now, rev});
            const auto request = S::inserted(s.db);
            S::run(
                s.db,
                "UPDATE ScheduledWorldEvents SET State='PENDING' WHERE WorldID=? AND ScheduledWorldEventID=?",
                {s.world, occurrence});
            venueFact(s, request, occurrence, p.event ? "VENUE_REVISED" : "VENUE_ANNOUNCED", rev);
        }
    }
    if(s.clockModel && s.god)
    {
        auto name=t.venueName;auto surface=t.surface;
        for(const auto& p:patches)if(p.change){if(p.field=="NAME")name=*p.desired;else surface=p.desired;}
        ClockStateSql::directVenue(s.db,s.world,t.venue,s.now,rev,s.context,name,surface);
    }
    result.changed = true;
    result.venues.push_back(t.venue);
}
