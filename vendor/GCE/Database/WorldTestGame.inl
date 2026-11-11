#include "GCE/Database/WorldStorageHash.h"
// Development-only single-game queue driver. Included beside trusted Session helpers.
namespace
{
constexpr const char *TestProvider = "PFF_GENERIC_QUEUE_TEST";
WorldEditCommit testCommit(Session &s)
{
    WorldEditCommit r;
    r.worldId = s.world;
    S::Query q(s.db, "SELECT WorldKey FROM Worlds WHERE WorldID=?", {s.world});
    q.row();
    r.worldKey = q.text();
    r.revision = r.committedRevision = s.revision;
    r.currentTimeUtc = s.now;
    r.changed = true;
    return r;
}
std::string testTime(Session &s, const std::string &kickoff, int elapsed)
{
    S::Query q(s.db, "SELECT strftime('%Y-%m-%dT%H:%M:%SZ',?,'+'||?||' seconds')",
               {kickoff, std::int64_t(elapsed)});
    q.row();
    return q.text();
}
std::int64_t testContext(Session &s, std::int64_t game, std::int64_t claim, const std::string &at,
                         std::int64_t rev, const std::string &key)
{
    auto order = S::scalar(s.db,
                           "SELECT COALESCE(max(OrderInInstant),0)+1 FROM WorldExecutionContexts WHERE "
                           "WorldID=? AND EffectiveUtc=?",
                           {s.world, at});
    S::run(s.db,
           "INSERT INTO "
           "WorldExecutionContexts(WorldID,ActorUserID,DriverKind,GameID,ClaimID,EffectiveUtc,WorldAnchorUtc,"
           "DataRevision,PhaseCode,CommandKey,OrderInInstant) VALUES(?,?,'GAME',?,?,?,?,?,3,?,?)",
           {s.world, s.user, game, claim, at, s.now, rev, key, order});
    return S::inserted(s.db);
}
void testDispatchDue(Session &s, const std::string &through, std::int64_t rev, std::int64_t request = 0,
                     std::int64_t game = 0, std::int64_t claim = 0, std::string *notices = nullptr)
{
    // Same typed dispatcher as ordinary World advancement; never invent handlers.
    for (;;)
    {
        S::Query q(s.db,
                   "SELECT "
                   "e.ScheduledWorldEventID,e.StartsUtc,e.KindKey,COALESCE(d.PolicyKey,''),k.HandlerVersion,"
                   "COALESCE(d.PolicyVersion,1) FROM ScheduledWorldEvents e JOIN WorldEventKinds k ON "
                   "k.WorldID=e.WorldID AND k.KindKey=e.KindKey LEFT JOIN SeasonEventInstances i ON "
                   "i.WorldID=e.WorldID AND i.ScheduledWorldEventID=e.ScheduledWorldEventID LEFT JOIN "
                   "SeasonEventDefinitions d ON d.WorldID=i.WorldID AND d.DefinitionID=i.DefinitionID WHERE "
                   "e.WorldID=? AND e.State='PENDING' AND e.KindKey<>'GAME_KICKOFF' AND e.StartsUtc<=? ORDER "
                   "BY e.StartsUtc,e.DispatchPriority,e.ScheduledWorldEventID",
                   {s.world, through});
        if (!q.row())
            break;
        const auto firstInstant = q.text(1);
        while (!clockDependenciesReady(s, q.integer()))
        {
            require(q.row() && q.text(1) == firstInstant, WorldEditError::DueWorkPending,
                    "Queue stopped: event prerequisites are not resolved");
        }
        const auto event = q.integer();
        const auto at = q.text(1);
        const auto handler = q.text(2) == "VENUE_APPLY" ? q.text(2) : q.text(3);
        require(clockDependenciesReady(s, event), WorldEditError::DueWorkPending,
                "Queue stopped: event prerequisites are not resolved");
        require(!S::scalar(s.db,
                           "SELECT count(*) FROM WorldEventParticipants p JOIN Characters c ON "
                           "c.WorldID=p.WorldID AND c.CharacterID=p.CharacterID AND c.PersonID=p.PersonID "
                           "AND c.WorldUserID=p.WorldUserID JOIN WorldUsers u ON u.WorldID=p.WorldID "
                           "AND u.WorldUserID=p.WorldUserID WHERE p.WorldID=? AND "
                           "p.ScheduledWorldEventID=? AND p.Required=1 AND p.State='OFFERED' AND "
                           "c.EndsUtc IS NULL AND u.Active=1 AND c.StartsUtc<=? AND p.OfferedStartUtc<=?",
                           {s.world, event, at, at}),
                WorldEditError::DueWorkPending, "Queue stopped: an event requires a user decision");
        const bool supported = q.integer(4) == 1 && q.integer(5) == 1 &&
            (handler == "VENUE_APPLY" || handler == "OPEN_TRADING" || handler == "CLOSE_TRADING");
        if (!game)
        {
            require(at >= s.now, WorldEditError::ClockBarrier, "Queue contains overdue work");
            S::run(s.db, "UPDATE Worlds SET CurrentTimeUtc=? WHERE WorldID=?", {at, s.world});
            s.now = at;
        }
        clockDispatch(s, request, event, supported ? handler : "SUPPORT_PENDING", rev, game, claim, at);
        if (notices && visibleEvent(s, event, at))
            *notices += supported ? "[WORLD QUEUE] " + handler + " applied at " + at + ".\n"
                                  : "[WORLD QUEUE] Event support pending at " + at + ".\n";
    }
}
std::int64_t testClaim(Session &s, std::int64_t game, std::int64_t generation, int elapsed,
                       const std::string &hash)
{
    S::run(s.db,
           "INSERT INTO "
           "GameExecutionClaims(WorldID,GameID,CommandKey,ExpectedGeneration,TargetElapsedSeconds,"
           "ProviderStepHash,InputBindingsHash,State) SELECT WorldID,GameID,?,?,?,?,InputBindingsHash,'OPEN' "
           "FROM GameRuns WHERE WorldID=? AND GameID=?",
           {std::string("test-step:") + std::to_string(generation + 1), generation, std::int64_t(elapsed),
            hash, s.world, game});
    return S::inserted(s.db);
}
void testCheckpoint(Session &s, std::int64_t game, std::int64_t claim, const WorldTestGameStep &p,
                    const WorldTestSavedState *saved = nullptr, bool paused = false,
                             const std::function<std::string(std::string_view)> &taggedOutput = {})
{
    const auto bytes = saved ? WorldTestStateCodec::encode(*saved) : p.text + p.teamStatistics;
    const auto hash = WorldStorageHash::bytes(bytes);
    const std::string format =
        saved ? WorldTestStateCodec::Format
              : (p.generation == 0 ? WorldTestStateCodec::InitialFormat : WorldTestStateCodec::StepFormat);
    S::run(s.db,
           "INSERT INTO "
           "GameExecutionCheckpoints(WorldID,GameID,Generation,ClaimID,PeriodNo,RemainingSeconds,"
           "ElapsedSeconds,State,FormatKey,FormatVersion,ContentHash,ResumeData) "
           "VALUES(?,?,?,?,?,?,?,?,?,1,?,?)",
           {s.world, game, p.generation, claim, std::int64_t(p.period), std::int64_t(p.remaining),
            std::int64_t(p.elapsed),
            std::string(p.final  ? "FINAL"
                        : paused ? "PAUSED"
                                 : "RUNNING"),
            format, hash, bytes});
    S::run(s.db, "UPDATE GameExecutionClaims SET State='COMMITTED' WHERE WorldID=? AND ClaimID=?",
           {s.world, claim});
    S::run(s.db,
           "INSERT INTO "
           "GameSharedEventHorizons(WorldID,GameID,Generation,ProducesNoSharedEvents,ProducerPolicyKey,"
           "ProducerPolicyVersion,ProofHash) VALUES(?,?,?,1,'QUEUE_TEST_NO_WORLD_CONSEQUENCES',1,?)",
           {s.world, game, p.generation, hash});
}
void testWrite(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << bytes;
    f.flush();
    require(bool(f), WorldEditError::IoError, "Cannot flush game artifact");
    f.close();
    require(bool(f), WorldEditError::IoError, "Cannot close game artifact");
}
std::int64_t testArtifact(Session &s, std::int64_t game, const std::filesystem::path &path,
                          const std::string &kind, const std::string &bytes)
{
    const auto relative = std::filesystem::path("PBP") / path.filename();
    S::run(s.db,
           "INSERT INTO "
           "GameArtifacts(GameID,ArtifactType,RelativePath,FormatKey,FormatVersion,HashAlgorithm,ContentHash,"
           "CreatedUtc) VALUES(?,?,?,'PFF_QUEUE_TEST_TEXT','1','FNV1A64',?,?)",
           {game, kind, relative.generic_u8string(), ClockTime::digest(bytes), s.now});
    return S::inserted(s.db);
}
} // namespace

namespace
{
bool savedTestGameReady(Session &, std::int64_t);
WorldTestSavedState readTestProgress(Session &, const WorldActor &, std::int64_t, bool final = false);
std::string readTestPbp(Session &, const WorldActor &, std::int64_t);
} // namespace
#include "GCE/Database/WorldGameAdmission.inl"
#include "GCE/Database/WorldGameInterruption.inl"
WorldEditResult<WorldTestMatchup> loadFinalTestMatchup(const WorldActor &actor, std::int64_t game)
{
    return attempt<WorldTestMatchup>([&] {
        Session s(actor, false);
        S::Query final(s.db, "SELECT Generation,FinalElapsedSeconds FROM GameFinalSaves "
                             "WHERE WorldID=? AND GameID=?", {s.world, game});
        require(final.row(), WorldEditError::InvalidValue, "Game has no saved final");
        const auto fields = admissionInput(s, actor, game);
        WorldTestMatchup result;
        result.gameId = game;
        result.fixtureId = std::stoll(fields[5]);
        result.kickoffUtc = fields[7];
        result.awayId = std::stoll(fields[8]);
        result.homeId = std::stoll(fields[9]);
        result.awayName = fields[10];
        result.homeName = fields[11];
        result.awayCoreKey = fields[12];
        result.homeCoreKey = fields[13];
        result.saved = readTestProgress(s, actor, game, true);
        result.pbpPath = WorldEditingAccess::path(actor).parent_path() / "PBP" /
                         ("Game-" + std::to_string(game) + ".pbp");
        require(result.saved->game.generation == final.integer() && result.saved->game.elapsed == final.integer(1),
                WorldEditError::IntegrityFailure, "Final checkpoint disagrees with the save receipt");
        S::exec(s.db, "COMMIT");
        return result;
    });
}
WorldEditResult<WorldTestMatchup> beginTestMatchup(const WorldActor &actor, std::int64_t season,
                                                   std::int64_t expected, const std::string &evidence)
{
    return attempt<WorldTestMatchup>([&] {
        std::int64_t window = 0;
        std::string admissionKey;
        {
            Session s(actor, false);
            admissionScope(s, season);
            require(s.revision == expected, WorldEditError::StaleRevision, "World changed; reload Schedule");
            window = admissionActive(s);
            if (!window)
                admissionKey =
                    WorldStorageHash::bytes(std::to_string(season) + admissionNext(s, season) + evidence)
                        .substr(0, 48);
            if (window)
            {
                S::Query selected(s.db,
                                  "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                                  {s.world, admissionSelectionKey(window)});
                if (selected.row())
                    require(savedTestGameReady(s, std::stoll(selected.text())),
                            WorldEditError::UnsupportedCodec,
                            "Game is already open or has an incompatible development checkpoint");
            }
            S::exec(s.db, "COMMIT");
        }
        if (!window)
        {
            auto admitted = beginNextGameWindow(actor, season, admissionKey, expected, evidence);
            require(admitted.success, admitted.error, admitted.message.c_str());
            if (admitted.value.reason != "WINDOW_ADMITTED")
            {
                std::string message = admitted.value.reason + " at " + admitted.value.currentTimeUtc;
                if (admitted.value.stopEventId)
                {
                    Session read(actor, false);
                    if (visibleEvent(read, admitted.value.stopEventId))
                    {
                        S::Query event(read.db,
                                       "SELECT EventKey FROM ScheduledWorldEvents WHERE WorldID=? AND "
                                       "ScheduledWorldEventID=?",
                                       {read.world, admitted.value.stopEventId});
                        if (event.row())
                            message += " - " + event.text();
                    }
                    S::exec(read.db, "COMMIT");
                }
                throw Failure(WorldEditError::DueWorkPending, message);
            }
            window = admitted.value.windowId;
            expected = admitted.value.revision;
        }
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        admissionScope(s, season);
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
        require(s.revision == expected, WorldEditError::StaleRevision,
                "World changed before selecting admitted game");
        require(admissionActive(s) == window, WorldEditError::ClockBarrier, "Admitted window changed");
        std::int64_t selectedGame = 0;
        {
            S::Query selected(s.db, "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                              {s.world, admissionSelectionKey(window)});
            if (selected.row())
            {
                selectedGame = std::stoll(selected.text());
                require(savedTestGameReady(s, selectedGame), WorldEditError::UnsupportedCodec,
                        "Game is already open or checkpoint is incompatible");
            }
        }
        S::Query fixture(
            s.db,
            "SELECT "
            "r.GameID,m.WorldScheduledGameID,r.ScheduledWorldEventID,r.KickoffUtc,w.CompetitionSeasonID,r."
            "InputBindingsHash FROM "
            "GameRuns r JOIN GameWindowMembers m ON m.WorldID=r.WorldID AND "
            "m.ScheduledWorldEventID=r.ScheduledWorldEventID JOIN GameProcessingWindows w ON "
            "w.WorldID=r.WorldID AND w.WindowID=r.WindowID WHERE r.WorldID=? AND r.WindowID=? AND "
            "r.State IN ('RUNNING','PAUSED') AND (?=0 OR r.GameID=?) AND NOT EXISTS(SELECT 1 FROM "
            "GameFinalSaves f WHERE f.WorldID=r.WorldID AND "
            "f.GameID=r.GameID) ORDER BY m.WorldScheduledGameID LIMIT 1",
            {s.world, window, selectedGame, selectedGame});
        require(fixture.row() && fixture.integer(4) == season, WorldEditError::IntegrityFailure,
                "No unfinished admitted matchup in selected season");
        WorldTestMatchup r;
        r.windowId = window;
        r.gameId = fixture.integer();
        r.fixtureId = fixture.integer(1);
        r.kickoffUtc = fixture.text(3);
        r.inputHash = fixture.text(5);
        auto fields = admissionInput(s, actor, r.gameId);
        require(fields[5] == std::to_string(r.fixtureId) && fields[6] == std::to_string(fixture.integer(2)) &&
                    fields[7] == r.kickoffUtc,
                WorldEditError::IntegrityFailure, "Input fixture binding mismatch");
        r.seed = static_cast<std::uint32_t>(std::stoull(fields[3]));
        r.awayId = std::stoll(fields[8]);
        r.homeId = std::stoll(fields[9]);
        r.awayName = fields[10];
        r.homeName = fields[11];
        r.awayCoreKey = fields[12];
        r.homeCoreKey = fields[13];
        if (selectedGame)
            r.saved = readTestProgress(s, actor, r.gameId);
        else
        {
            S::Query initial(s.db,
                             "SELECT FormatKey FROM GameExecutionCheckpoints WHERE WorldID=? AND GameID=? "
                             "AND Generation=0",
                             {s.world, r.gameId});
            require(initial.row() && initial.text() == WorldTestStateCodec::InitialFormat,
                    WorldEditError::UnsupportedCodec,
                    "Obsolete development checkpoint; create a fresh league");
        }
        r.pbpPath = WorldEditingAccess::path(actor).parent_path() / "PBP" /
                    ("Game-" + std::to_string(r.gameId) + ".pbp");
        require(!std::filesystem::exists(r.pbpPath), WorldEditError::IoError, "Final PBP already exists");
        r.queuedEvents = testQueuedEvents(s, r.gameId);
        S::Query interrupted(s.db, "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                              {s.world, testInterruptionKey(r.gameId)});
        if (interrupted.row())
        {
            r.pauseThrough = std::stoi(interrupted.text());
            require(selectedGame && r.saved && r.pauseThrough >= r.saved->game.elapsed && r.pauseThrough <= 4500,
                    WorldEditError::IntegrityFailure, "Invalid saved interruption boundary");
            r.interruption = testInterruption(s, r.gameId, r.pauseThrough);
            if (r.interruption)
            {
                r.commit = testCommit(s);
                r.commit.changed = false;
                tx.commit();
                return r; // Repeated resume never writes or silently accepts the event.
            }
            S::run(s.db, "DELETE FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                   {s.world, testInterruptionKey(r.gameId)});
            r.pauseThrough = -1;
        }
        if (!selectedGame)
            S::run(s.db, "INSERT INTO WorldSettings VALUES(?,?,?)",
                   {s.world, admissionSelectionKey(window), std::to_string(r.gameId)});
        else
        {
            S::run(s.db,
                   "UPDATE GameRuns SET State='RUNNING' WHERE WorldID=? AND GameID=? AND State='PAUSED'",
                   {s.world, r.gameId});
            S::run(s.db,
                   "UPDATE GameProcessingWindows SET State='ADMITTED' WHERE WorldID=? AND WindowID=? AND "
                   "State='PAUSED'",
                   {s.world, window});
        }
        const auto rev = s.revision + 1;
        schedulerRevision(s, rev);
        r.commit = testCommit(s);
        tx.commit();
        return r;
    });
}

namespace
{
void persistBufferedTestStep(Session &s, std::int64_t game, const WorldTestGameStep &supplied,
                             std::int64_t rev, const std::filesystem::path &path, std::string &pbp,
                             const WorldTestSavedState *saved = nullptr, bool paused = false,
                             const std::function<std::string(std::string_view)> &taggedOutput = {})
{
    auto p = supplied;
    require(s.clockModel && p.generation > 0 && p.period >= 1 && p.period <= 5 && p.remaining >= 0 &&
                p.remaining <= 900 && p.elapsed >= 0 && p.awayScore >= 0 && p.homeScore >= 0,
            WorldEditError::InvalidValue, "Invalid test game checkpoint");
    S::Query run(s.db,
                 "SELECT "
                 "r.KickoffUtc,r.ScheduledWorldEventID,r.WindowID,g.AwayTeamSeasonID,g.HomeTeamSeasonID,"
                 "g.WorldScheduledGameID,g.CompetitionSeasonID FROM GameRuns r JOIN Games g ON "
                 "g.WorldID=r.WorldID AND g.GameID=r.GameID WHERE r.WorldID=? AND r.GameID=? AND "
                 "r.ProviderKey=? AND r.State='RUNNING'",
                 {s.world, game, std::string(TestProvider)});
    require(run.row(), WorldEditError::InvalidValue, "No active test game");
    const auto kickoff = run.text(), at = testTime(s, kickoff, p.elapsed);
    const auto event = run.integer(1), window = run.integer(2);
    S::Query old(s.db,
                 "SELECT Generation,PeriodNo,RemainingSeconds,ElapsedSeconds FROM GameHistoricalProgress "
                 "WHERE WorldID=? AND GameID=?",
                 {s.world, game});
    require(old.row() && old.integer() + 1 == p.generation, WorldEditError::StaleRevision,
            "Play already committed or game generation changed");
    require((p.period == old.integer(1) && p.remaining <= old.integer(2) &&
             p.elapsed - old.integer(3) == old.integer(2) - p.remaining) ||
                (p.period == old.integer(1) + 1 && old.integer(2) == 0 && p.remaining == 900 &&
                 p.elapsed == old.integer(3)),
            WorldEditError::InvalidValue, "Invalid quarter or timing transition");
    require(p.final == ((p.period == 4 && p.remaining == 0 && p.awayScore != p.homeScore) || (p.period == 5 && (p.remaining == 0 || p.awayScore != p.homeScore))), WorldEditError::InvalidValue,
            "Invalid final game checkpoint");
    if (p.period == 5 && old.integer(1) == 4)
        S::run(s.db, "INSERT INTO GamePeriods VALUES(?,?,5,'OVERTIME','COUNTDOWN',900,'PFF_QUEUE_TEST_TIMING')",
               {s.world, game});
    const auto claim =
        testClaim(s, game, p.generation - 1, p.elapsed,
                  WorldStorageHash::bytes(p.text + p.teamStatistics +
                                          (saved ? WorldTestStateCodec::encode(*saved) : std::string{})));
    std::string notices;
    testDispatchDue(s, at, rev, 0, game, claim, &notices);
    p.text += notices;
    const auto context = testContext(
        s, game, claim, at, rev, "test-step:" + std::to_string(game) + ":" + std::to_string(p.generation));
    if (p.elapsed > old.integer(3))
    {
        S::run(s.db,
               "INSERT INTO "
               "GameTimingIntervals(WorldID,GameID,ClaimID,StartsElapsedSeconds,EndsElapsedSeconds,"
               "PeriodNo,ClockBeforeSeconds,ClockAfterSeconds,Certainty,SourceRef) "
               "VALUES(?,?,?,?,?,?,?,?,'SIMULATED','PFF_QUEUE_TEST_TIMING')",
               {s.world, game, claim, old.integer(3), std::int64_t(p.elapsed), std::int64_t(p.period),
                old.integer(2), std::int64_t(p.remaining)});
        if (p.play)
        {
            require(p.playOrdinal == S::scalar(s.db,
                                               "SELECT COALESCE(max(PlayOrdinal),0)+1 FROM GamePlayTiming "
                                               "WHERE WorldID=? AND GameID=?",
                                               {s.world, game}),
                    WorldEditError::IntegrityFailure, "Play order must continue the saved canonical history");
            S::run(s.db,
                   "INSERT INTO "
                   "GamePlayTiming(WorldID,GameID,PlayOrdinal,CanonicalPlayKey,PeriodNo,RemainingSeconds,"
                   "StartsElapsedSeconds,EndsElapsedSeconds,Certainty) VALUES(?,?,?,?,?,?,?,?,'SIMULATED')",
                   {s.world, game, p.playOrdinal, "test-play:" + std::to_string(p.playOrdinal),
                    std::int64_t(p.period), old.integer(2), old.integer(3), std::int64_t(p.elapsed)});
        }
    }
    pbp += p.text;
    std::optional<WorldTestSavedState> finalState;
    if (p.final && saved)
    {
        finalState = *saved;
        finalState->replayHistory = taggedOutput(pbp);
        testWrite(path, finalState->replayHistory);
        S::run(s.db, "INSERT INTO GameArtifacts(GameID,ArtifactType,RelativePath,FormatKey,FormatVersion,"
                     "HashAlgorithm,ContentHash,CreatedUtc) VALUES(?,'TAGGED_PBP',?,'PFF_LIVE_PBP','2','SHA256',?,?)",
               {game, (std::filesystem::path("PBP") / path.filename()).generic_u8string(),
                WorldStorageHash::bytes(finalState->replayHistory), s.now});
        saved = &*finalState;
    }
    testCheckpoint(s, game, claim, p, saved, paused);
    S::run(s.db,
           "INSERT INTO "
           "CausalReadFences(WorldID,ContextID,ResourceKind,ResourceKey,AsOfUtc,VersionHash,AsOfOrder) "
           "SELECT ?,?,'EVENT_QUEUE','WORLD',?,'QUEUE_TEST_V1',OrderInInstant FROM "
           "WorldExecutionContexts WHERE WorldID=? AND ContextID=?",
           {s.world, context, at, s.world, context});
    const auto winner = p.awayScore > p.homeScore   ? run.integer(3)
                        : p.homeScore > p.awayScore ? run.integer(4)
                                                    : 0;
    S::run(s.db,
           "UPDATE Games SET AwayScore=?,HomeScore=?,WinnerTeamSeasonID=?,GameStatus=?,CompletedUtc=? "
           "WHERE WorldID=? AND GameID=?",
           {std::int64_t(p.awayScore), std::int64_t(p.homeScore),
            winner ? S::Value(winner) : S::Value(nullptr), std::string(p.final ? "FINAL" : "IN_PROGRESS"),
            p.final ? S::Value(at) : S::Value(nullptr), s.world, game});
    if (p.final)
    {
        const auto &bytes = saved ? saved->replayHistory : pbp;
        const auto stats = path.parent_path() / ("Game-" + std::to_string(game) + ".stats.txt");
        testWrite(stats, p.teamStatistics);
        testArtifact(s, game, stats, "TEAM_STATISTICS", p.teamStatistics);
        const auto hash = ClockTime::digest(bytes + p.teamStatistics);
        S::run(s.db, "INSERT INTO GameFinalSaves VALUES(?,?,?,?,?,?,?,?,?)",
               {s.world, game, p.generation, std::int64_t(p.elapsed), at,
                "test-final:" + std::to_string(game), hash, hash, rev});
        S::run(s.db, "UPDATE GameRuns SET State='FINAL' WHERE WorldID=? AND GameID=?", {s.world, game});
        S::run(s.db,
               "UPDATE ScheduledWorldEvents SET "
               "State='COMPLETED',ResolvedUtc=?,ResolvedRevision=?,ResolutionReason='QUEUE_TEST_"
               "FINAL' WHERE WorldID=? AND ScheduledWorldEventID=?",
               {at, rev, s.world, event});
        S::run(s.db, "INSERT INTO SchedulerDispatchReceipts VALUES(?,?,'COMPLETE',?,?,?,?)",
               {s.world, event, context, hash, at, rev});
        S::run(s.db,
               "INSERT INTO "
               "FinalizedLeagueGameResults(WorldID,WorldScheduledGameID,GameID,FinalizationKey,"
               "ResultFingerprint,CompletedUtc,FinalizedRevision,ExecutionContextID) "
               "VALUES(?,?,?,?,?,?,?,?)",
               {s.world, run.integer(5), game, "test-final:" + std::to_string(game), hash, at, rev, context});
        // Week closure is result-driven, with no World-clock jump.
        S::Query week(s.db,
                      "SELECT w.LeagueWeekID,w.NextLeagueWeekID FROM LeagueWeeks w JOIN "
                      "LeagueWeekGames m ON m.WorldID=w.WorldID AND m.LeagueWeekID=w.LeagueWeekID "
                      "WHERE m.WorldID=? AND m.WorldScheduledGameID=? AND w.State='OPEN'",
                      {s.world, run.integer(5)});
        if (week.row() &&
            !S::scalar(s.db,
                       "SELECT count(*) FROM LeagueWeekGames m WHERE m.WorldID=? AND "
                       "m.LeagueWeekID=? AND NOT EXISTS(SELECT 1 FROM FinalizedLeagueGameResults f "
                       "WHERE f.WorldID=m.WorldID AND f.WorldScheduledGameID=m.WorldScheduledGameID)",
                       {s.world, week.integer()}))
        {
            S::Query end(
                s.db,
                "SELECT max(f.CompletedUtc) FROM FinalizedLeagueGameResults f JOIN LeagueWeekGames m "
                "ON m.WorldID=f.WorldID AND m.WorldScheduledGameID=f.WorldScheduledGameID WHERE "
                "m.WorldID=? AND m.LeagueWeekID=?",
                {s.world, week.integer()});
            end.row();
            const auto close = end.text();
            S::run(s.db,
                   "UPDATE LeagueWeeks SET State='CLOSED',ClosedUtc=?,ClosedRevision=? WHERE "
                   "WorldID=? AND LeagueWeekID=?",
                   {close, rev, s.world, week.integer()});
            S::run(s.db,
                   "INSERT INTO "
                   "LeagueWeekTransitions(WorldID,CompetitionSeasonID,FromLeagueWeekID,"
                   "ToLeagueWeekID,TriggerGameID,OccurredUtc,DataRevision) VALUES(?,?,?,?,?,?,?)",
                   {s.world, run.integer(6), week.integer(),
                    week.integer(1) ? S::Value(week.integer(1)) : S::Value(nullptr), game, close, rev});
            if (week.integer(1))
                S::run(s.db,
                       "UPDATE LeagueWeeks SET State='OPEN',OpenedUtc=?,OpenedRevision=? WHERE "
                       "WorldID=? AND LeagueWeekID=? AND State='FUTURE'",
                       {close, rev, s.world, week.integer(1)});
        }
        if (!S::scalar(s.db,
                       "SELECT count(*) FROM GameWindowMembers m WHERE m.WorldID=? AND m.WindowID=? "
                       "AND NOT EXISTS(SELECT 1 FROM GameRuns r JOIN GameFinalSaves f ON "
                       "f.WorldID=r.WorldID AND f.GameID=r.GameID WHERE r.WorldID=m.WorldID AND "
                       "r.ScheduledWorldEventID=m.ScheduledWorldEventID AND r.State='FINAL')",
                       {s.world, window}))
            S::run(s.db,
                   "UPDATE GameProcessingWindows SET State='RESOLVED',ResolvedRevision=? WHERE "
                   "WorldID=? AND WindowID=?",
                   {rev, s.world, window});
    }
}
} // namespace

#include "GCE/Database/WorldGameProgress.inl"
WorldEditResult<WorldEditCommit> finalizeTestMatchup(const WorldActor &actor, std::int64_t game,
                                                     const std::vector<WorldTestGameStep> &steps,
                                                     std::int64_t expected, std::string *recordedText,
                                                     const WorldTestSavedState *saved,
                                                     const std::function<std::string(std::string_view)> &taggedOutput)
{
    return attempt<WorldEditCommit>([&] {
        require(!steps.empty() && steps.back().final, WorldEditError::InvalidValue,
                "Complete game output is required before finalization");
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
        require(s.revision == expected, WorldEditError::StaleRevision,
                "World changed during the in-memory game; buffered output is retained for inspection");
        const auto frozen = admissionInput(s, actor, game);
        S::Query season(s.db, "SELECT CompetitionSeasonID FROM Games WHERE WorldID=? AND GameID=?",
                        {s.world, game});
        require(season.row(), WorldEditError::InvalidValue, "Game is outside this World");
        admissionVerifyProvider(s, actor, season.integer(), frozen[2]);
        if (saved)
            validateTestSnapshot(s, game, steps.back(), *saved, true);
        std::string pbp = readTestPbp(s, actor, game);
        const auto folder = WorldEditingAccess::path(actor).parent_path() / "PBP";
        const auto path = folder / ("Game-" + std::to_string(game) + ".pbp");
        const auto stats = folder / ("Game-" + std::to_string(game) + ".stats.txt");
        require(!std::filesystem::exists(path) && !std::filesystem::exists(stats), WorldEditError::IoError,
                "Final artifacts already exist; they will not be overwritten");
        require(!saved || bool(taggedOutput), WorldEditError::InvalidValue,
                "Final game requires tagged PBP");
        struct Cleanup
        {
            std::filesystem::path pbp, stats;
            bool published = false;
            ~Cleanup()
            {
                if (!published)
                {
                    std::error_code e;
                    std::filesystem::remove(pbp, e);
                    std::filesystem::remove(stats, e);
                }
            }
        } cleanup{path, stats};
        const auto rev = s.revision + 1;
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            require(steps[i].final == (i + 1 == steps.size()), WorldEditError::InvalidValue,
                    "Only the last buffered step may be final");
            persistBufferedTestStep(s, game, steps[i], rev, path, pbp,
                                    (saved && i + 1 == steps.size()) ? saved : nullptr, false, taggedOutput);
        }
        S::run(
            s.db,
            "DELETE FROM WorldSettings WHERE WorldID=? AND SettingKey=(SELECT "
            "'Admission.Executing:'||WindowID FROM GameRuns WHERE WorldID=? AND GameID=?) AND SettingValue=?",
            {s.world, s.world, game, std::to_string(game)});
        schedulerRevision(s, rev);
        auto result = testCommit(s);
        tx.commit();
        cleanup.published = true;
        if (recordedText)
            *recordedText = std::move(pbp);
        return result;
    });
}
