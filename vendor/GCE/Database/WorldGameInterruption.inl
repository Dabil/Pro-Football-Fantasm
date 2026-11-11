// GAME-INTERRUPTION-TASK4-2026-11-04. Trusted queue boundaries for the reduced
// provider. Pause at its last safe football checkpoint, before a crossing play.
namespace
{
// Static admission-time proof that pending prerequisites can be dispatched in
// this horizon. Never mark ordinary automatic chains as user interruptions.
bool testAutomaticChain(Session &s, std::int64_t event, const std::string &through, std::vector<std::int64_t> &visiting)
{
    if (visiting.size() >= 128 || std::find(visiting.begin(), visiting.end(), event) != visiting.end())
        return false;
    S::Query q(
        s.db,
        "SELECT e.State,e.StartsUtc,e.KindKey,k.HandlerVersion,COALESCE(d.PolicyKey,''),"
        "COALESCE(d.PolicyVersion,1),EXISTS(SELECT 1 FROM WorldEventParticipants p JOIN Characters c "
        "ON c.WorldID=p.WorldID AND c.CharacterID=p.CharacterID AND c.PersonID=p.PersonID AND "
        "c.WorldUserID=p.WorldUserID JOIN WorldUsers u ON u.WorldID=p.WorldID AND "
        "u.WorldUserID=p.WorldUserID WHERE p.WorldID=e.WorldID AND p.ScheduledWorldEventID=e.ScheduledWorldEventID "
        "AND p.Required=1 AND p.State='OFFERED' AND c.EndsUtc IS NULL AND u.Active=1 AND "
        "c.StartsUtc<=e.StartsUtc AND p.OfferedStartUtc<=e.StartsUtc),e.ResolutionReason FROM ScheduledWorldEvents e "
        "JOIN WorldEventKinds k ON k.WorldID=e.WorldID AND k.KindKey=e.KindKey LEFT JOIN SeasonEventInstances i "
        "ON i.WorldID=e.WorldID AND i.ScheduledWorldEventID=e.ScheduledWorldEventID LEFT JOIN SeasonEventDefinitions d "
        "ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE e.WorldID=? AND e.ScheduledWorldEventID=?",
        {s.world, event});
    if (!q.row())
        return false;
    if (q.text() == "COMPLETED")
        return q.text(7) != "SUPPORT_PENDING";
    const auto handler = q.text(2) == "VENUE_APPLY" ? q.text(2) : q.text(4);
    if (q.text() != "PENDING" || q.text(1) > through || q.integer(3) != 1 || q.integer(5) != 1 ||
        q.integer(6) || (handler != "VENUE_APPLY" && handler != "OPEN_TRADING" && handler != "CLOSE_TRADING"))
        return false;
    // A source dependency without an installed occurrence cannot be guessed.
    if (S::scalar(s.db,
                  "SELECT count(*) FROM SeasonEventInstances child JOIN WorldSettings x ON "
                  "x.WorldID=child.WorldID AND x.SettingKey LIKE 'Clock.SourceDependency:'||child.DefinitionID||':%' "
                  "JOIN SeasonEventDefinitions parent ON parent.WorldID=child.WorldID AND "
                  "x.SettingKey='Clock.SourceDependency:'||child.DefinitionID||':'||parent.DefinitionID "
                  "WHERE child.WorldID=? AND child.ScheduledWorldEventID=? AND NOT EXISTS(SELECT 1 FROM "
                  "SeasonEventInstances p WHERE p.WorldID=parent.WorldID AND p.DefinitionID=parent.DefinitionID)",
                  {s.world, event}))
        return false;
    visiting.push_back(event);
    S::Query parents(s.db,
                     "SELECT RequiresEventID FROM EventDependencies WHERE WorldID=? AND ScheduledWorldEventID=? "
                     "UNION SELECT p.ScheduledWorldEventID FROM SeasonEventInstances child JOIN WorldSettings x ON "
                     "x.WorldID=child.WorldID JOIN SeasonEventDefinitions parent ON parent.WorldID=child.WorldID AND "
                     "x.SettingKey='Clock.SourceDependency:'||child.DefinitionID||':'||parent.DefinitionID JOIN "
                     "SeasonEventInstances p "
                     "ON p.WorldID=parent.WorldID AND p.DefinitionID=parent.DefinitionID WHERE child.WorldID=? AND "
                     "child.ScheduledWorldEventID=?",
                     {s.world, event, s.world, event});
    bool ready = true;
    while (parents.row())
        if (!testAutomaticChain(s, parents.integer(), q.text(1), visiting))
        {
            ready = false;
            break;
        }
    visiting.pop_back();
    return ready;
}
std::string testInterruptionKey(std::int64_t game)
{
    return "GameTime.Interruption:" + std::to_string(game);
}
std::vector<WorldTestQueuedEvent> testQueuedEvents(Session &s, std::int64_t game)
{
    S::Query run(s.db, "SELECT KickoffUtc FROM GameRuns WHERE WorldID=? AND GameID=?", {s.world, game});
    require(run.row(), WorldEditError::InvalidValue, "Missing admitted game");
    const auto kickoff = run.text();
    S::Query q(s.db,
               "SELECT e.ScheduledWorldEventID,e.StartsUtc,e.KindKey,COALESCE(d.PolicyKey,''),"
               "k.HandlerVersion,COALESCE(d.PolicyVersion,1),"
               "CAST(strftime('%s',e.StartsUtc) AS INTEGER)-CAST(strftime('%s',?) AS INTEGER),"
               "e.EventKey,EXISTS(SELECT 1 FROM WorldEventParticipants p JOIN Characters c ON "
               "c.WorldID=p.WorldID AND c.CharacterID=p.CharacterID AND c.PersonID=p.PersonID AND "
               "c.WorldUserID=p.WorldUserID JOIN WorldUsers u ON u.WorldID=p.WorldID AND "
               "u.WorldUserID=p.WorldUserID WHERE p.WorldID=e.WorldID AND "
               "p.ScheduledWorldEventID=e.ScheduledWorldEventID AND p.Required=1 AND p.State='OFFERED' "
               "AND c.StartsUtc<=e.StartsUtc AND c.EndsUtc IS NULL AND u.Active=1 AND "
               "p.OfferedStartUtc<=e.StartsUtc) FROM ScheduledWorldEvents e JOIN WorldEventKinds k ON "
               "k.WorldID=e.WorldID AND k.KindKey=e.KindKey LEFT JOIN SeasonEventInstances i ON "
               "i.WorldID=e.WorldID AND i.ScheduledWorldEventID=e.ScheduledWorldEventID LEFT JOIN "
               "SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID "
               "WHERE e.WorldID=? AND e.State='PENDING' AND e.KindKey<>'GAME_KICKOFF' AND "
               "e.StartsUtc<=? ORDER BY e.StartsUtc,e.DispatchPriority,k.PhaseCode,e.ScheduledWorldEventID",
               {kickoff, s.world, testTime(s, kickoff, 4500)});
    std::vector<WorldTestQueuedEvent> result;
    while (q.row())
    {
        WorldTestQueuedEvent event;
        event.elapsedSeconds = int(q.integer(6));
        event.startsUtc = q.text(1);
        const auto handler = q.text(2) == "VENUE_APPLY" ? q.text(2) : q.text(3);
        std::vector<std::int64_t> visiting;
        if (!clockDependenciesReady(s, q.integer()) && !testAutomaticChain(s, q.integer(), q.text(1), visiting))
        {
            event.reason = "UNSATISFIED_DEPENDENCY";
            event.blocker = "An event is waiting for a prerequisite";
        }
        else if (q.integer(8))
        {
            event.reason = "REQUIRED_ATTENTION";
            event.blocker = "An event requires a user decision";
        }
        const bool supported = q.integer(4) == 1 && q.integer(5) == 1 &&
            (handler == "VENUE_APPLY" || handler == "OPEN_TRADING" || handler == "CLOSE_TRADING");
        if (visibleEvent(s, q.integer(), event.startsUtc))
        {
            if (!event.blocker.empty())
                event.blocker += ": " + q.text(7) + " at " + event.startsUtc;
            event.notice = supported ? "[WORLD QUEUE] " + handler + " due at " + event.startsUtc + " (pending save).\n"
                                     : "[WORLD QUEUE] Event support pending at " + event.startsUtc + " (pending save).\n";
        }
        // Hidden occurrences do not expose their key, ID, policy, or timestamp.
        else
            event.startsUtc.clear();
        result.push_back(std::move(event));
    }
    return result;
}
std::optional<WorldTestQueuedEvent> testInterruption(Session &s, std::int64_t game, int through)
{
    for (const auto &event : testQueuedEvents(s, game))
        if (event.elapsedSeconds <= through && !event.blocker.empty())
            return event;
    return {};
}
} // namespace
