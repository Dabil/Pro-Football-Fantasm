// Included beside trusted Session helpers; no standalone connection/provider API.
namespace
{
bool controlsGameOffer(Session& s, std::int64_t participation, std::int64_t controller = 0);
bool canAdvance(Session& s)
{
    return S::scalar(s.db,
                     "SELECT CanAdvanceWorld FROM WorldUsers WHERE WorldID=? AND WorldUserID=? AND Active=1",
                     {s.world, s.user}) == 1;
}
bool visibleEvent(Session& s, std::int64_t id, const std::string& asOf = {})
{
    const auto& knownAt = asOf.empty() ? s.now : asOf;
    return S::scalar(
               s.db,
               "SELECT count(*) FROM ScheduledWorldEvents e WHERE e.WorldID=? AND e.ScheduledWorldEventID=? "
               "AND e.CreatedUtc<=? AND (e.Visibility='PUBLIC' OR (e.Visibility='RESTRICTED' AND EXISTS "
               "(SELECT 1 FROM WorldEventReaders r WHERE r.WorldID=e.WorldID AND "
               "r.ScheduledWorldEventID=e.ScheduledWorldEventID "
               "AND r.WorldUserID=? AND r.KnownFromUtc<=?)))",
               {s.world, id, knownAt, s.user, knownAt}) == 1;
}
void schedulerReady(Session& s)
{
    require(S::utc(s.now) && worldEventContractsReady(s.db, s.world) &&
                worldEventPublicationReady(s.db, s.world),
            WorldEditError::IntegrityFailure, "Invalid World event publication or contract");
}
void schedulerRevision(Session& s, std::int64_t rev)
{
    require(s.revision < std::numeric_limits<std::int64_t>::max() && rev == s.revision + 1,
            WorldEditError::IntegrityFailure, "World revision exhausted");
    if (!s.clockModel) schedulerReady(s);
    S::run(s.db, "UPDATE WorldDataRevisions SET Revision=? WHERE WorldID=? AND Revision=?",
           {rev, s.world, s.revision});
    require(sqlite3_changes(s.db) == 1, WorldEditError::StaleRevision, "World revision changed");
    s.revision = rev;
    if (s.clockModel) schedulerReady(s); // Contexts/effects carry the newly published revision.
}
WorldAdvanceOutcome advanceOutcome(Session& s, std::int64_t id)
{
    S::Query q(s.db,
               "SELECT State,StopReason,PauseEventID,ChangedRevision,CreatedRevision FROM "
               "WorldAdvanceRequests WHERE "
               "WorldID=? AND AdvanceRequestID=?",
               {s.world, id});
    require(q.row(), WorldEditError::InvalidValue, "Unknown World advancement request");
    WorldAdvanceOutcome r;
    r.requestId = id;
    r.revision = s.revision;
    r.currentTimeUtc = s.now;
    r.state = q.text();
    r.reason = q.text(1);
    r.committedRevision = q.integer(4);
    r.outcomeRevision = q.integer(3);
    if(s.clockModel){S::Query window(s.db,"SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",{s.world,"Admission.Request:"+std::to_string(id)});if(window.row())r.windowId=std::stoll(window.text());}
    if (!q.null(2))
    {
        if (visibleEvent(s, q.integer(2)))
            r.stopEventId = q.integer(2);
        else
            r.reason = "WAITING_FOR_WORLD_WORK";
    }
    return r;
}
void advanceState(Session& s, std::int64_t id, const std::string& state, const std::string& reason,
                  std::int64_t pause, int phase, std::int64_t rev)
{
    S::Query old(
        s.db, "SELECT CursorUtc,CursorPhase FROM WorldAdvanceRequests WHERE WorldID=? AND AdvanceRequestID=?",
        {s.world, id});
    require(old.row(), WorldEditError::InvalidValue, "Unknown World advancement request");
    if (old.text() == s.now)
        phase = std::max(phase, static_cast<int>(old.integer(1)));
    S::run(s.db,
           "UPDATE WorldAdvanceRequests SET State=?,StopReason=?,PauseEventID=?,CursorUtc=?,CursorPhase=?,"
           "LastAdvancedByUserID=?,ChangedRevision=? WHERE WorldID=? AND AdvanceRequestID=?",
           {state, reason.empty() ? S::Value(nullptr) : S::Value(reason),
            pause ? S::Value(pause) : S::Value(nullptr), s.now, static_cast<std::int64_t>(phase), s.user, rev,
            s.world, id});
}
void applyVenueDue(Session& s, std::int64_t occurrence, std::int64_t request, std::int64_t rev)
{
    S::Query q(
        s.db,
        "SELECT v.VenueChangeEventID,v.WorldVenueID,v.FieldCode,v.DesiredValue,p.DueUtc,v.PlanID "
        "FROM VenueChangeEvents v JOIN VenueChangePlans p ON p.WorldID=v.WorldID AND p.PlanID=v.PlanID "
        "WHERE v.WorldID=? AND v.ScheduledWorldEventID=? AND v.State='PENDING'",
        {s.world, occurrence});
    require(q.row() && q.text(4) == s.now, WorldEditError::ClockBarrier,
            "Venue work must execute at its stored deadline");
    const auto field = q.text(2);
    const auto desired = q.optionalText(3);
    require(field == "NAME" || field == "SURFACE", WorldEditError::UnsupportedHandler,
            "Unsupported venue field");
    if (field == "NAME")
        require(desired && text(*desired, 256, true) == *desired, WorldEditError::IntegrityFailure,
                "Invalid pending venue name");
    else
        require(optionalText(desired, 128) == desired, WorldEditError::IntegrityFailure,
                "Invalid pending venue surface");
    S::Query venue(s.db, "SELECT VenueName,TurfType FROM WorldVenues WHERE WorldID=? AND WorldVenueID=?",
                   {s.world, q.integer(1)});
    require(venue.row(), WorldEditError::IntegrityFailure, "Venue no longer exists");
    const auto before = venue.optionalText(field == "NAME" ? 0 : 1);
    advanceState(s, request, "RUNNING", "", 0, 1, rev);
    S::run(s.db,
           field == "NAME" ? "UPDATE WorldVenues SET VenueName=? WHERE WorldID=? AND WorldVenueID=?"
                           : "UPDATE WorldVenues SET TurfType=? WHERE WorldID=? AND WorldVenueID=?",
           {S::nullable(desired), s.world, q.integer(1)});
    S::run(s.db,
           "UPDATE VenueChangeEvents SET State='APPLIED',ResolvedUtc=?,ResolvedRevision=?,"
           "ResolutionReason='DUE_TIME',AppliedBeforeValue=? WHERE WorldID=? AND VenueChangeEventID=? AND "
           "State='PENDING'",
           {s.now, rev, S::nullable(before), s.world, q.integer()});
    require(sqlite3_changes(s.db) == 1, WorldEditError::StalePendingEvent, "Due venue request changed");
    S::run(s.db,
           "UPDATE ScheduledWorldEvents SET State='COMPLETED',StartedUtc=?,ResolvedUtc=?,ResolvedRevision=?,"
           "ResolutionReason='DUE_TIME' WHERE WorldID=? AND ScheduledWorldEventID=? AND State='PENDING'",
           {s.now, s.now, rev, s.world, occurrence});
    std::string fingerprint = "venue-apply-v1:";
    fingerprintPart(fingerprint, std::to_string(q.integer()));
    fingerprintPart(fingerprint, std::to_string(q.integer(5)));
    fingerprintOptional(fingerprint, desired);
    S::run(s.db, "INSERT INTO SchedulerDispatchReceipts VALUES(?,?,'APPLY',?,?,?,?)",
           {s.world, occurrence, request, fingerprint, s.now, rev});
    S::run(s.db,
           "INSERT INTO "
           "WorldEventFacts(WorldID,FactKey,FactKind,ScheduledWorldEventID,VenueChangeEventID,OccurredUtc,"
           "DataRevision,Visibility) "
           "SELECT ?,?,'VENUE_APPLIED',ScheduledWorldEventID,?,?,?,Visibility FROM ScheduledWorldEvents "
           "WHERE WorldID=? AND ScheduledWorldEventID=?",
           {s.world, "venue:" + std::to_string(q.integer()) + ":VENUE_APPLIED", q.integer(), s.now, rev,
            s.world, occurrence});
    const auto fact = S::inserted(s.db);
    S::run(s.db,
           "INSERT INTO WorldFactReaders(WorldID,FactID,WorldUserID,KnownFromUtc) "
           "SELECT r.WorldID,?,r.WorldUserID,CASE WHEN r.KnownFromUtc>? THEN r.KnownFromUtc ELSE ? END "
           "FROM WorldEventReaders r JOIN ScheduledWorldEvents e ON e.WorldID=r.WorldID AND "
           "e.ScheduledWorldEventID=r.ScheduledWorldEventID "
           "WHERE r.WorldID=? AND r.ScheduledWorldEventID=? AND e.Visibility='RESTRICTED'",
           {fact, s.now, s.now, s.world, occurrence});
}
#include "GCE/Database/WorldClockAdvance.inl"

// One bounded, atomic unit. Game admission/execution remains a separate capability.
WorldAdvanceOutcome advanceUnit(const WorldActor& actor, std::int64_t id, std::int64_t expected)
{
    Session s(actor, true);
    S::Transaction tx(s.db);
    s.refresh();
    schedulerReady(s);
    require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission is required");
    auto out = advanceOutcome(s, id);
    if (out.state == "COMPLETED" || out.state == "CANCELED" || s.revision != expected)
    {
        out.stale = s.revision != expected;
        tx.commit();
        return out;
    }
    require(s.revision < std::numeric_limits<std::int64_t>::max(), WorldEditError::IntegrityFailure,
            "World revision exhausted");
    const auto rev = s.revision + 1;
    if (s.clockModel) return clockAdvanceUnit(s, tx, id, rev);
    S::Query intent(s.db,
                    "SELECT Mode,TargetUtc,VisibleStopEventID FROM WorldAdvanceRequests WHERE WorldID=? AND "
                    "AdvanceRequestID=?",
                    {s.world, id});
    require(intent.row(), WorldEditError::InvalidValue, "Unknown World advancement request");
    const bool next = intent.text() == "NEXT_EVENT";
    bool changedTarget = false;
    std::string target = intent.text(1);
    if (next)
    {
        S::Query anchor(
            s.db,
            "SELECT StartsUtc,State FROM ScheduledWorldEvents WHERE WorldID=? AND ScheduledWorldEventID=?",
            {s.world, intent.integer(2)});
        require(anchor.row(), WorldEditError::IntegrityFailure, "Missing next-event anchor");
        target = anchor.text();
        changedTarget = anchor.text(1) == "CANCELED" || anchor.text(1) == "SUPERSEDED";
    }
    const auto finish = [&](const std::string& state, const std::string& reason, std::int64_t event,
                            int phase) {
        advanceState(s, id, state, reason, event, phase, rev);
        schedulerRevision(s, rev);
        auto r = advanceOutcome(s, id);
        r.units = 1;
        tx.commit();
        return r;
    };
    // Without a validated codec/provider, a running game is a clock barrier.
    S::Query running(s.db,
                     "SELECT ScheduledWorldEventID FROM ScheduledWorldEvents WHERE WorldID=? AND "
                     "State='RUNNING' ORDER BY StartsUtc,ScheduledWorldEventID LIMIT 1",
                     {s.world});
    if (running.row())
        return finish("BLOCKED", "UNSUPPORTED_CODEC", running.integer(), 0);
    S::Query unbound(
        s.db,
        "SELECT g.GameID FROM Games g WHERE g.WorldID=? AND g.GameStatus='IN_PROGRESS' AND NOT EXISTS "
        "(SELECT 1 FROM RunningGameEvents r JOIN ScheduledWorldEvents e ON e.WorldID=r.WorldID AND "
        "e.ScheduledWorldEventID=r.ScheduledWorldEventID "
        "WHERE r.WorldID=g.WorldID AND r.GameID=g.GameID AND e.State='RUNNING') LIMIT 1",
        {s.world});
    if (unbound.row())
        return finish("BLOCKED", "CLOCK_BARRIER", 0, 0);
    S::Query overdue(s.db,
                     "SELECT ScheduledWorldEventID FROM ScheduledWorldEvents WHERE WorldID=? AND "
                     "State='PENDING' AND StartsUtc<? ORDER BY StartsUtc,ScheduledWorldEventID LIMIT 1",
                     {s.world, s.now});
    if (overdue.row())
        return finish("BLOCKED", "CLOCK_BARRIER", overdue.integer(), 0);
    S::Query due(s.db,
                 "SELECT e.ScheduledWorldEventID,e.KindKey,k.HandlerVersion FROM ScheduledWorldEvents e JOIN "
                 "WorldEventKinds k "
                 "ON k.WorldID=e.WorldID AND k.KindKey=e.KindKey WHERE e.WorldID=? AND e.State='PENDING' AND "
                 "e.StartsUtc=? "
                 "AND k.PhaseCode=1 ORDER BY e.ScheduledWorldEventID LIMIT 1",
                 {s.world, s.now});
    if (due.row())
    {
        if (due.text(1) != "VENUE_APPLY" || due.integer(2) != 1)
            return finish("BLOCKED", "UNSUPPORTED_HANDLER", due.integer(), 1);
        applyVenueDue(s, due.integer(), id, rev);
        schedulerRevision(s, rev);
        auto r = advanceOutcome(s, id);
        r.units = 1;
        tx.commit();
        return r;
    }
    if (changedTarget)
        return finish("COMPLETED", "TARGET_CHANGED", 0, 1);
    S::Query offers(s.db,
                    "SELECT p.ParticipationID,p.WorldUserID,p.ScheduledWorldEventID FROM "
                    "WorldEventParticipants p JOIN ScheduledWorldEvents e "
                    "ON e.WorldID=p.WorldID AND e.ScheduledWorldEventID=p.ScheduledWorldEventID WHERE "
                    "p.WorldID=? AND e.KindKey='GAME_KICKOFF' "
                    "AND e.State='PENDING' AND e.StartsUtc=? AND p.State IN "
                    "('OFFERED','ATTENDING','DECLINED','DELEGATED') ORDER BY p.ParticipationID",
                    {s.world, s.now});
    while (offers.row())
        if (!controlsGameOffer(s, offers.integer(), offers.integer(1)))
            return finish("BLOCKED", "STALE_CHOICE", offers.integer(2), 2);
    S::Query choice(
        s.db,
        "SELECT p.ScheduledWorldEventID FROM WorldEventParticipants p JOIN ScheduledWorldEvents e "
        "ON e.WorldID=p.WorldID AND e.ScheduledWorldEventID=p.ScheduledWorldEventID WHERE p.WorldID=? AND "
        "e.State='PENDING' "
        "AND e.KindKey='GAME_KICKOFF' AND p.Required=1 AND p.State='OFFERED' AND e.StartsUtc=? ORDER BY "
        "p.ScheduledWorldEventID,p.ParticipationID LIMIT 1",
        {s.world, s.now});
    if (choice.row())
        return finish("PAUSED", "REQUIRED_DECISION", choice.integer(), 2);
    S::Query start(s.db,
                   "SELECT ScheduledWorldEventID,KindKey FROM ScheduledWorldEvents WHERE WorldID=? AND "
                   "State='PENDING' AND StartsUtc=? ORDER BY ScheduledWorldEventID LIMIT 1",
                   {s.world, s.now});
    if (start.row())
        return finish("BLOCKED",
                      start.text(1) == "GAME_KICKOFF" ? "UNSUPPORTED_PROVIDER" : "UNSUPPORTED_HANDLER",
                      start.integer(), 3);
    if (target <= s.now)
        return finish("COMPLETED", "TARGET_REACHED", 0, 3);
    S::Query boundary(
        s.db,
        "SELECT min(StartsUtc) FROM ScheduledWorldEvents WHERE WorldID=? AND State='PENDING' AND StartsUtc>?",
        {s.world, s.now});
    boundary.row();
    const auto nextTime = boundary.text();
    const auto to = nextTime.empty() || nextTime > target ? target : nextTime;
    require(S::utc(to) && to > s.now, WorldEditError::ClockBarrier, "Invalid next World boundary");
    S::run(s.db, "UPDATE Worlds SET CurrentTimeUtc=? WHERE WorldID=?", {to, s.world});
    s.now = to;
    return finish("RUNNING", "", 0, 0);
}
} // namespace

WorldEditResult<WorldAdvanceOutcome> loadWorldAdvance(const WorldActor& actor, std::int64_t id)
{
    return attempt<WorldAdvanceOutcome>([&] {
        Session s(actor, false);
        auto r = advanceOutcome(s, id);
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<WorldAdvanceOutcome> beginWorldAdvance(const WorldActor& actor, const std::string& key,
                                                       WorldAdvanceMode mode,
                                                       const std::optional<std::string>& target,
                                                       std::int64_t expected)
{
    return attempt<WorldAdvanceOutcome>([&] {
        require(mode == WorldAdvanceMode::NextEvent || mode == WorldAdvanceMode::ToTime,
                WorldEditError::InvalidValue, "Invalid advancement mode");
        const auto command = text(key, 128, true);
        const bool next = mode == WorldAdvanceMode::NextEvent;
        require(next ? !target : (target && S::utc(*target)), WorldEditError::InvalidValue,
                "Target UTC does not match advancement mode");
        std::string fingerprint = "world-advance-v1:";
        fingerprintPart(fingerprint, next ? "NEXT_EVENT" : "TO_TIME");
        fingerprintOptional(fingerprint, target);
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        schedulerReady(s);
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission is required");
        S::Query replay(
            s.db,
            "SELECT AdvanceRequestID,RequestFingerprint,StartedByUserID FROM WorldAdvanceRequests WHERE "
            "WorldID=? AND CommandKey=?",
            {s.world, command});
        if (replay.row())
        {
            require(replay.integer(2) == s.user && replay.text(1) == fingerprint,
                    WorldEditError::IdempotencyConflict,
                    "Advancement command key already has different content");
            auto r = advanceOutcome(s, replay.integer());
            r.replayed = true;
            tx.commit();
            return r;
        }
        require(s.revision == expected, WorldEditError::StaleRevision,
                "The World changed; reload advancement state");
        require(!target || *target >= s.now, WorldEditError::InvalidValue, "Cannot advance to the past");
        require(S::scalar(s.db,
                          "SELECT count(*) FROM WorldAdvanceRequests WHERE WorldID=? AND State IN "
                          "('RUNNING','PAUSED','BLOCKED')",
                          {s.world}) == 0,
                WorldEditError::AdvanceAlreadyActive,
                "Resume or cancel the existing World advancement request");
        std::int64_t anchor = 0;
        if (next)
        {
            S::Query candidates(s.db,
                                "SELECT ScheduledWorldEventID FROM ScheduledWorldEvents WHERE WorldID=? AND "
                                "(State IN ('PENDING','RUNNING') OR (?=1 AND State='COMPLETED' AND "
                                "CASE WHEN KindKey='GAME_KICKOFF' THEN ResolvedUtc ELSE StartsUtc END>?)) "
                                "ORDER BY CASE WHEN ?=1 AND State='COMPLETED' AND KindKey='GAME_KICKOFF' "
                                "THEN ResolvedUtc ELSE StartsUtc END,ScheduledWorldEventID",
                                {s.world, std::int64_t(s.clockModel), s.now, std::int64_t(s.clockModel)});
            while (candidates.row())
                if (visibleEvent(s, candidates.integer()))
                {
                    anchor = candidates.integer();
                    break;
                }
        }
        require(s.revision < std::numeric_limits<std::int64_t>::max(), WorldEditError::IntegrityFailure,
                "World revision exhausted");
        const auto rev = s.revision + 1;
        S::run(
            s.db,
            "INSERT INTO "
            "WorldAdvanceRequests(WorldID,StartedByUserID,LastAdvancedByUserID,CommandKey,RequestFingerprint,"
            "Mode,TargetUtc,VisibleStopEventID,OrderingPolicyKey,State,CursorUtc,CursorPhase,CreatedRevision,"
            "ChangedRevision) "
            "VALUES(?,?,?,?,?,?,?,?,'WORLD_TIME_V1','RUNNING',?,0,?,?)",
            {s.world, s.user, s.user, command, fingerprint, std::string(next ? "NEXT_EVENT" : "TO_TIME"),
             S::nullable(target), anchor ? S::Value(anchor) : S::Value(nullptr), s.now, rev, rev});
        const auto id = S::inserted(s.db);
        if (next && !anchor)
            advanceState(s, id, "COMPLETED", "NO_VISIBLE_EVENT", 0, 0, rev);
        schedulerRevision(s, rev);
        auto r = advanceOutcome(s, id);
        tx.commit();
        return r;
    });
}
WorldEditResult<WorldAdvanceOutcome> continueWorldAdvance(const WorldActor& actor, std::int64_t id,
                                                          std::int64_t expected, std::size_t budget)
{
    return attempt<WorldAdvanceOutcome>([&] {
        require(budget > 0 && budget <= 64, WorldEditError::InvalidValue,
                "Advancement budget must be 1 through 64");
        WorldAdvanceOutcome result;
        std::size_t units = 0;
        const auto yieldAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(25);
        while (units < budget)
        {
            try
            {
                result = advanceUnit(actor, id, expected);
            }
            catch (const std::exception& error)
            {
                // The failed unit rolled back before recording a durable, sanitized blocker.
                Session s(actor, true);
                S::Transaction tx(s.db);
                s.refresh();
                schedulerReady(s);
                require(canAdvance(s), WorldEditError::NotAuthorized,
                        "World advancement permission is required");
                result = advanceOutcome(s, id);
                if (result.state == "COMPLETED" || result.state == "CANCELED" || s.revision != expected)
                {
                    result.stale = s.revision != expected;
                    tx.commit();
                    break;
                }
                require(s.revision < std::numeric_limits<std::int64_t>::max(),
                        WorldEditError::IntegrityFailure, "World revision exhausted");
                const auto rev = s.revision + 1;
                const std::string reason = s.clockModel && std::string(error.what()).find("CAUSAL_FENCE")!=std::string::npos
                    ? "CAUSAL_FENCE" : "DISPATCH_FAILED";
                advanceState(s, id, "BLOCKED", reason, 0, 0, rev);
                schedulerRevision(s, rev);
                result = advanceOutcome(s, id);
                result.units = 1;
                tx.commit();
            }
            units += result.units;
            expected = result.revision;
            if (result.state != "RUNNING" || result.stale || !result.units ||
                std::chrono::steady_clock::now() >= yieldAt)
                break;
        }
        result.units = units;
        return result;
    });
}
WorldEditResult<WorldAdvanceOutcome> cancelWorldAdvance(const WorldActor& actor, std::int64_t id,
                                                        std::int64_t expected)
{
    return attempt<WorldAdvanceOutcome>([&] {
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        schedulerReady(s);
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission is required");
        auto r = advanceOutcome(s, id);
        if (r.state != "COMPLETED" && r.state != "CANCELED")
        {
            require(s.revision == expected, WorldEditError::StaleRevision,
                    "The World changed; reload advancement state");
            require(s.revision < std::numeric_limits<std::int64_t>::max(), WorldEditError::IntegrityFailure,
                    "World revision exhausted");
            const auto rev = s.revision + 1;
            advanceState(s, id, "CANCELED", "USER_CANCELED", 0, 0, rev);
            schedulerRevision(s, rev);
            r = advanceOutcome(s, id);
        }
        tx.commit();
        return r;
    });
}

WorldEditResult<WorldTimeline> loadWorldTimeline(const WorldActor& actor, const std::string& from,
                                                 const std::string& to, std::size_t offset, std::size_t limit)
{
    return attempt<WorldTimeline>([&] {
        require(S::utc(from) && S::utc(to) && from <= to && limit > 0 && limit <= 500 && offset <= 1000000,
                WorldEditError::InvalidValue, "Invalid timeline range or page");
        Session s(actor, false);
        WorldTimeline r;
        r.revision = s.revision;
        r.currentTimeUtc = s.now;
        r.canAdvance = canAdvance(s);
        std::string sql=
                   "SELECT e.ScheduledWorldEventID,e.KindKey,e.StartsUtc,e.EndsUtc,e.EndMode,e.State "
                   "FROM ScheduledWorldEvents e WHERE e.WorldID=? AND e.State IN ('PENDING','RUNNING') AND "
                   "e.StartsUtc>=? AND e.StartsUtc<=? "
                   "AND e.CreatedUtc<=? AND (e.Visibility='PUBLIC' OR (e.Visibility='RESTRICTED' AND EXISTS "
                   "(SELECT 1 FROM WorldEventReaders v WHERE v.WorldID=e.WorldID AND "
                   "v.ScheduledWorldEventID=e.ScheduledWorldEventID "
                   "AND v.WorldUserID=? AND v.KnownFromUtc<=?))) ORDER BY "
                   "e.StartsUtc,e.ScheduledWorldEventID LIMIT ? OFFSET ?";
        if(s.clockModel) {
            const std::string h="(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=e.WorldID)";
            auto at=sql.find("e.State ");
            sql.replace(at,8,"CASE WHEN e.ResolvedUtc>"+h+" THEN CASE WHEN e.KindKey='GAME_KICKOFF' AND e.StartedUtc<="+h+" THEN 'RUNNING' ELSE 'PENDING' END ELSE e.State END ");
            at=sql.find("e.State IN ('PENDING','RUNNING')");
            sql.replace(at,std::string("e.State IN ('PENDING','RUNNING')").size(),"(e.State IN ('PENDING','RUNNING') OR (e.State='COMPLETED' AND e.ResolvedUtc>"+h+"))");
            r.canAdvance=false;
        }
        S::Query q(s.db,sql.c_str(),{s.world, from, to, s.now, s.user, s.now, static_cast<std::int64_t>(limit),static_cast<std::int64_t>(offset)});
        while (q.row())
            r.events.push_back({q.integer(), q.text(1), q.text(2), q.text(3), q.text(4), q.text(5)});
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<std::vector<WorldLeagueNewsEntry>> loadWorldLeagueNews(
    const WorldActor& actor, std::int64_t season)
{
    return attempt<std::vector<WorldLeagueNewsEntry>>([&] {
        Session s(actor, false);
        std::vector<WorldLeagueNewsEntry> entries;
        const std::string completed =
            "SELECT e.ResolvedUtc,e.EventKey,e.ResolutionReason FROM ScheduledWorldEvents e "
            "WHERE e.WorldID=? AND e.Visibility='PUBLIC' AND e.State='COMPLETED' "
            "AND e.KindKey<>'GAME_KICKOFF' AND e.ResolvedUtc<=?";
        const std::string seasonFilter =
            " AND NOT EXISTS(SELECT 1 FROM SeasonEventInstances i JOIN SeasonEventDefinitions d "
            "ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE i.WorldID=e.WorldID "
            "AND i.ScheduledWorldEventID=e.ScheduledWorldEventID AND d.CompetitionSeasonID<>?)";
        const auto sql = completed + (s.clockModel ? seasonFilter : " AND ? IS NOT NULL") +
            " ORDER BY e.ScheduledWorldEventID";
        S::Query events(s.db,sql.c_str(),{s.world,s.now,season});
        while (events.row())
        {
            auto headline = events.text(1);
            if (events.text(2) == "SUPPORT_PENDING") headline += " (support pending)";
            entries.push_back({events.text(),std::move(headline)});
        }
        S::Query games(s.db,
            "SELECT g.CompletedUtc,away.DisplayName,g.AwayScore,home.DisplayName,g.HomeScore "
            "FROM Games g JOIN WorldScheduledGames fixture ON fixture.WorldScheduledGameID=g.WorldScheduledGameID "
            "JOIN TeamSeasons away ON away.TeamSeasonID=fixture.AwayTeamSeasonID "
            "JOIN TeamSeasons home ON home.TeamSeasonID=fixture.HomeTeamSeasonID "
            "WHERE g.WorldID=? AND fixture.CompetitionSeasonID=? AND g.GameStatus='FINAL' "
            "AND g.CompletedUtc<=? AND g.AwayScore IS NOT NULL AND g.HomeScore IS NOT NULL "
            "AND NOT EXISTS(SELECT 1 FROM GameKickoffOccurrences k JOIN ScheduledWorldEvents e "
            "ON e.WorldID=k.WorldID AND e.ScheduledWorldEventID=k.ScheduledWorldEventID "
            "WHERE k.WorldID=g.WorldID AND k.WorldScheduledGameID=fixture.WorldScheduledGameID "
            "AND k.IsCurrent=1 AND e.Visibility<>'PUBLIC') ORDER BY g.GameID", {s.world,season,s.now});
        while (games.row())
            entries.push_back({games.text(), "Final: " + games.text(1) + " " + std::to_string(games.integer(2))
                + " at " + games.text(3) + " " + std::to_string(games.integer(4))});
        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            return left.occurredUtc > right.occurredUtc;
        });
        S::exec(s.db,"COMMIT");
        return entries;
    });
}

WorldEditResult<std::vector<WorldEventFact>> loadWorldFacts(const WorldActor& actor, const std::string& from,
                                                            const std::string& to, std::size_t offset,
                                                            std::size_t limit)
{
    return attempt<std::vector<WorldEventFact>>([&] {
        require(S::utc(from) && S::utc(to) && from <= to && limit > 0 && limit <= 500 && offset <= 1000000,
                WorldEditError::InvalidValue, "Invalid fact range or page");
        Session s(actor, false);
        require(to <= s.now, WorldEditError::InvalidValue, "Fact reveal time cannot exceed the World clock");
        S::Query q(
            s.db,
            "SELECT f.FactID,f.ScheduledWorldEventID,f.VenueChangeEventID,f.GameID,f.FactKind,f.OccurredUtc "
            "FROM WorldEventFacts f WHERE f.WorldID=? AND f.OccurredUtc>=? AND f.OccurredUtc<=? "
            "AND (f.Visibility='PUBLIC' OR (f.Visibility='RESTRICTED' AND EXISTS "
            "(SELECT 1 FROM WorldFactReaders r WHERE r.WorldID=f.WorldID AND r.FactID=f.FactID AND "
            "r.WorldUserID=? AND r.KnownFromUtc<=?))) "
            "ORDER BY f.OccurredUtc,f.FactID LIMIT ? OFFSET ?",
            {s.world, from, to, s.user, to, static_cast<std::int64_t>(limit),
             static_cast<std::int64_t>(offset)});
        std::vector<WorldEventFact> r;
        while (q.row())
        {
            // A fact's audience may disclose the fact without disclosing a private occurrence ID.
            r.push_back({q.integer(), visibleEvent(s, q.integer(1), to) ? q.integer(1) : 0, q.integer(2),
                         q.integer(3), q.text(4), q.text(5)});
        }
        S::exec(s.db, "COMMIT");
        return r;
    });
}
namespace
{
bool controlsGameOffer(Session& s, std::int64_t participation, std::int64_t controller)
{
    return S::scalar(s.db, R"SQL(
SELECT count(*) FROM WorldEventParticipants p JOIN Characters c ON c.WorldID=p.WorldID AND c.CharacterID=p.CharacterID
JOIN GameKickoffOccurrences k ON k.WorldID=p.WorldID AND k.ScheduledWorldEventID=p.ScheduledWorldEventID AND k.IsCurrent=1
JOIN WorldScheduledGames g ON g.WorldScheduledGameID=k.WorldScheduledGameID
JOIN WorldUsers owner ON owner.WorldID=p.WorldID AND owner.WorldUserID=p.WorldUserID AND owner.Active=1
JOIN CompetitionSeasons cs ON cs.CompetitionSeasonID=g.CompetitionSeasonID AND cs.Status='ACTIVE'
JOIN WorldCompetitions wc ON wc.WorldCompetitionID=cs.WorldCompetitionID AND wc.Active=1
WHERE p.WorldID=? AND p.ParticipationID=? AND p.WorldUserID=? AND c.WorldUserID=p.WorldUserID
 AND c.PersonID=p.PersonID AND c.EndsUtc IS NULL AND EXISTS (
 SELECT 1 FROM CharacterRoleControls rc JOIN RoleAssignments a ON a.WorldID=rc.WorldID AND a.RoleAssignmentID=rc.RoleAssignmentID
 JOIN RolePositions pos ON pos.WorldID=a.WorldID AND pos.PositionID=a.PositionID
 JOIN TeamSeasons t ON t.WorldOrganizationID=pos.WorldOrganizationID AND t.CompetitionSeasonID=g.CompetitionSeasonID
 JOIN WorldOrganizations org ON org.WorldID=p.WorldID AND org.WorldOrganizationID=t.WorldOrganizationID AND org.Active=1
 WHERE rc.WorldID=p.WorldID AND rc.CharacterID=p.CharacterID AND rc.WorldUserID=p.WorldUserID AND rc.EndsUtc IS NULL
 AND a.EndsUtc IS NULL AND pos.Active=1 AND (pos.TeamSeasonID IS NULL OR pos.TeamSeasonID=t.TeamSeasonID)
 AND t.TeamSeasonID IN (g.AwayTeamSeasonID,g.HomeTeamSeasonID) AND t.Status='ACTIVE')
)SQL",
                     {s.world, participation, controller ? controller : s.user}) == 1;
}
} // namespace
WorldEditResult<std::vector<WorldEventChoiceOffer>> loadWorldEventChoices(const WorldActor& actor)
{
    return attempt<std::vector<WorldEventChoiceOffer>>([&] {
        Session s(actor, false);
        std::vector<WorldEventChoiceOffer> r;
        S::Query q(s.db,
                   "SELECT "
                   "p.ParticipationID,p.ScheduledWorldEventID,p.PersonID,p.OfferedStartUtc,p.OfferedEndUtc,p."
                   "EndMode,p.State,p.Required "
                   "FROM WorldEventParticipants p JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID AND "
                   "e.ScheduledWorldEventID=p.ScheduledWorldEventID "
                   "WHERE p.WorldID=? AND p.WorldUserID=? AND p.State='OFFERED' AND e.State='PENDING' AND "
                   "e.StartsUtc=? "
                   "ORDER BY p.ParticipationID",
                   {s.world, s.user, s.now});
        while (q.row())
            if (visibleEvent(s, q.integer(1)) && controlsGameOffer(s, q.integer()))
                r.push_back({q.integer(), q.integer(1), q.integer(2), q.text(3), q.text(4), q.text(5),
                             q.text(6), q.integer(7) != 0});
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<WorldEditCommit> submitEventChoices(const WorldActor& actor, const std::string& key,
                                                    const std::vector<WorldEventChoice>& input,
                                                    std::int64_t expected)
{
    return attempt<WorldEditCommit>([&] {
        const auto command = text(key, 128, true);
        auto choices = input;
        require(!choices.empty() && choices.size() <= 64, WorldEditError::InvalidValue,
                "Choose 1 through 64 offers");
        std::sort(choices.begin(), choices.end(),
                  [](const auto& a, const auto& b) { return a.participationId < b.participationId; });
        std::string fingerprint = "event-choice-v1:";
        std::int64_t last = 0;
        for (const auto& c : choices)
        {
            require(c.participationId > last, WorldEditError::InvalidValue,
                    "Duplicate or invalid participation choice");
            last = c.participationId;
            require(c.choice == WorldEventChoiceKind::Attend || c.choice == WorldEventChoiceKind::Decline ||
                        c.choice == WorldEventChoiceKind::Delegate,
                    WorldEditError::InvalidValue, "Invalid choice");
            fingerprintPart(fingerprint, std::to_string(c.participationId));
            fingerprintPart(fingerprint, std::to_string(static_cast<int>(c.choice)));
        }
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        schedulerReady(s);
        WorldEditCommit r;
        require(!s.clockModel, WorldEditError::UnsupportedFeature,
                "World5 participation choices require the upcoming window/participation service");
        r.worldId = s.world;
        r.worldKey = WorldEditingAccess::worldKey(actor);
        r.revision = s.revision;
        r.currentTimeUtc = s.now;
        S::Query replay(s.db,
                        "SELECT RequestFingerprint,DataRevision FROM WorldParticipationCommands WHERE "
                        "WorldID=? AND WorldUserID=? AND CommandKey=?",
                        {s.world, s.user, command});
        if (replay.row())
        {
            require(replay.text() == fingerprint, WorldEditError::IdempotencyConflict,
                    "Choice command key already has different content");
            r.replayed = true;
            r.committedRevision = replay.integer(1);
            tx.commit();
            return r;
        }
        require(s.revision == expected, WorldEditError::StaleRevision,
                "The World changed; reload event choices");
        require(s.revision < std::numeric_limits<std::int64_t>::max(), WorldEditError::IntegrityFailure,
                "World revision exhausted");
        const auto rev = s.revision + 1;
        S::run(s.db,
               "INSERT INTO "
               "WorldParticipationCommands(WorldID,WorldUserID,CommandKey,RequestFingerprint,OccurredUtc,"
               "DataRevision) VALUES(?,?,?,?,?,?)",
               {s.world, s.user, command, fingerprint, s.now, rev});
        const auto commandId = S::inserted(s.db);
        for (const auto& c : choices)
        {
            require(controlsGameOffer(s, c.participationId), WorldEditError::NotAuthorized,
                    "Current control of the offered character's game role is required");
            S::Query q(s.db,
                       "SELECT "
                       "p.PersonID,p.Required,p.OfferedStartUtc,p.OfferedEndUtc,p.State,e.State,e.StartsUtc,"
                       "e.KindKey,p.ScheduledWorldEventID "
                       "FROM WorldEventParticipants p JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID AND "
                       "e.ScheduledWorldEventID=p.ScheduledWorldEventID "
                       "WHERE p.WorldID=? AND p.ParticipationID=?",
                       {s.world, c.participationId});
            require(q.row() && q.text(4) == "OFFERED" && q.text(5) == "PENDING" && q.text(6) == s.now &&
                        q.text(2) == s.now,
                    WorldEditError::StaleChoice, "Offer is no longer an available current-time decision");
            require(visibleEvent(s, q.integer(8)), WorldEditError::NotAuthorized,
                    "The offered event is not known to this user");
            require(c.choice != WorldEventChoiceKind::Delegate, WorldEditError::UnsupportedHandler,
                    "Game delegation has no installed handler");
            require(!q.integer(1) || c.choice == WorldEventChoiceKind::Attend, WorldEditError::InvalidValue,
                    "Required participation cannot be declined");
            if (c.choice == WorldEventChoiceKind::Attend)
                require(S::scalar(s.db,
                                  "SELECT count(*) FROM WorldAttendanceReservations WHERE WorldID=? AND "
                                  "PersonID=? AND State='HELD' "
                                  "AND StartsUtc<COALESCE(?,'9999-12-31T23:59:59Z') AND "
                                  "?<COALESCE(EndsUtc,'9999-12-31T23:59:59Z')",
                                  {s.world, q.integer(), S::nullable(q.optionalText(3)), q.text(2)}) == 0,
                        WorldEditError::AttendanceConflict, "This person already has overlapping attendance");
            S::run(s.db,
                   "UPDATE WorldEventParticipants SET "
                   "State=?,DecisionCommandID=?,DecisionUtc=?,DecisionRevision=? WHERE WorldID=? AND "
                   "ParticipationID=? AND State='OFFERED'",
                   {std::string(c.choice == WorldEventChoiceKind::Attend ? "ATTENDING" : "DECLINED"),
                    commandId, s.now, rev, s.world, c.participationId});
            if (c.choice == WorldEventChoiceKind::Attend)
                S::run(s.db,
                       "INSERT INTO "
                       "WorldAttendanceReservations(WorldID,ParticipationID,PersonID,StartsUtc,EndsUtc,State)"
                       " VALUES(?,?,?,?,?,'HELD')",
                       {s.world, c.participationId, q.integer(), q.text(2), S::nullable(q.optionalText(3))});
            r.people.push_back(q.integer());
        }
        schedulerRevision(s, rev);
        r.changed = true;
        r.revision = rev;
        r.committedRevision = rev;
        tx.commit();
        return r;
    });
}

WorldEditResult<WorldGameWindow> loadNextGameWindow(const WorldActor& actor,std::int64_t season)
{
    return attempt<WorldGameWindow>([&]{Session s(actor,false);require(s.clockModel,WorldEditError::UnsupportedSchema,"Obsolete development schema; create a fresh World5 league");auto result=previewInstalledWindow(s,season);S::exec(s.db,"COMMIT");return result;});
}
WorldEditResult<WorldAdvanceOutcome> beginNextGameWindow(const WorldActor& actor,std::int64_t season,const std::string& key,std::int64_t expected,const std::string& providerInputs)
{return beginInstalledGameWindow(actor,season,key,expected,providerInputs);}
