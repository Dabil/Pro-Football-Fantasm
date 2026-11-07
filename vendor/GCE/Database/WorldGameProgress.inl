// GAME-EXECUTION-TASK3-2026-11-04. Enlisted saved-progress operations; never
// invoked per play. Resume codecs own their complete runtime state.
namespace
{
bool savedTestGameReady(Session &s, std::int64_t game)
{
    return S::scalar(
               s.db,
               "SELECT count(*) FROM GameRuns r JOIN GameExecutionCheckpoints p ON p.WorldID=r.WorldID AND "
               "p.GameID=r.GameID WHERE r.WorldID=? AND r.GameID=? AND r.State='PAUSED' AND "
               "p.Generation=(SELECT max(Generation) FROM GameExecutionCheckpoints WHERE WorldID=r.WorldID "
               "AND GameID=r.GameID) AND p.State='PAUSED' AND p.FormatKey=? AND p.FormatVersion=1",
               {s.world, game, std::string(WorldTestStateCodec::Format)}) == 1;
}
std::string readTestPbp(Session &s, const WorldActor &actor, std::int64_t game)
{
    S::Query latest(s.db,
                    "SELECT Generation,FormatKey,FormatVersion,ContentHash,ResumeData FROM "
                    "GameExecutionCheckpoints WHERE WorldID=? AND GameID=? ORDER BY Generation DESC LIMIT 1",
                    {s.world, game});
    require(latest.row(), WorldEditError::UnsupportedCodec, "Missing game checkpoint");
    require(latest.integer(2) == 1 && WorldStorageHash::bytes(latest.text(4)) == latest.text(3),
            WorldEditError::IntegrityFailure, "Checkpoint checksum mismatch");
    if (latest.integer() == 0)
    {
        require(latest.text(1) == WorldTestStateCodec::InitialFormat, WorldEditError::UnsupportedCodec,
                "Obsolete development checkpoint; create a fresh league");
        return latest.text(4);
    }
    require(latest.text(1) == WorldTestStateCodec::Format, WorldEditError::UnsupportedCodec,
            "INCOMPATIBLE_CHECKPOINT: unsupported resume codec");
    S::Query artifact(s.db,
                      "SELECT a.RelativePath,a.ContentHash FROM GameArtifacts a JOIN Games g ON "
                      "g.GameID=a.GameID WHERE g.WorldID=? AND a.GameID=? AND a.ArtifactType=? AND "
                      "a.FormatKey='GAME_TIME_PBP_V1' AND a.FormatVersion='1' AND a.HashAlgorithm='SHA256'",
                      {s.world, game, "PROGRESS_PBP:" + std::to_string(latest.integer())});
    require(artifact.row(), WorldEditError::IntegrityFailure, "Missing saved play history");
    const auto relative = std::filesystem::u8path(artifact.text());
    require(!relative.is_absolute() && relative.parent_path() == "PBP" &&
                relative.filename() == "Progress-" + artifact.text(1) + ".txt",
            WorldEditError::IntegrityFailure, "Invalid progress artifact path");
    auto bytes = admissionBytes(WorldEditingAccess::path(actor).parent_path() / relative);
    require(WorldStorageHash::bytes(bytes) == artifact.text(1), WorldEditError::IntegrityFailure,
            "Saved play-history checksum mismatch");
    return bytes;
}
WorldTestSavedState readTestProgress(Session &s, const WorldActor &actor, std::int64_t game, bool final)
{
    S::Query q(
        s.db,
        "SELECT "
        "p.Generation,p.PeriodNo,p.RemainingSeconds,p.ElapsedSeconds,p.ResumeData,p.ContentHash,p.FormatKey,"
        "p.FormatVersion,r.InputBindingsHash,g.AwayScore,g.HomeScore FROM GameExecutionCheckpoints p JOIN "
        "GameRuns r ON r.WorldID=p.WorldID AND r.GameID=p.GameID JOIN Games g ON g.WorldID=r.WorldID AND "
        "g.GameID=r.GameID WHERE p.WorldID=? AND p.GameID=? ORDER BY p.Generation DESC LIMIT 1",
        {s.world, game});
    require(q.row() && q.text(6) == WorldTestStateCodec::Format && q.integer(7) == 1,
            WorldEditError::UnsupportedCodec, "INCOMPATIBLE_CHECKPOINT: unsupported GameTime save");
    require(WorldStorageHash::bytes(q.text(4)) == q.text(5), WorldEditError::IntegrityFailure,
            "Saved runtime checksum mismatch");
    WorldTestSavedState result;
    try
    {
        result = WorldTestStateCodec::decode(q.text(4));
    }
    catch (const std::exception &e)
    {
        throw Failure(WorldEditError::UnsupportedCodec, e.what());
    }
    const auto &g = result.game;
    require(
        g.playOrdinal ==
            S::scalar(s.db,
                      "SELECT COALESCE(max(PlayOrdinal),0) FROM GamePlayTiming WHERE WorldID=? AND GameID=?",
                      {s.world, game}),
        WorldEditError::IntegrityFailure, "Saved play sequence disagrees with canonical history");
    require(result.gameId == game && result.inputHash == q.text(8) && g.generation == q.integer() &&
                g.quarter == q.integer(1) && g.remaining == q.integer(2) && g.elapsed == q.integer(3) &&
                g.final == final && g.teams[0].score() == q.integer(9) && g.teams[1].score() == q.integer(10),
            WorldEditError::IntegrityFailure, "Saved state disagrees with game/checkpoint bindings");
    const auto frozen = admissionInput(s, actor, game);
    S::Query season(s.db, "SELECT CompetitionSeasonID FROM Games WHERE WorldID=? AND GameID=?",
                    {s.world, game});
    season.row();
    admissionVerifyProvider(s, actor, season.integer(), frozen[2]);
    if (final)
    {
        const auto filename = "Game-" + std::to_string(game) + ".pbp.txt";
        const auto relative = std::filesystem::path("PBP") / filename;
        S::Query artifact(s.db, "SELECT RelativePath,ContentHash FROM GameArtifacts WHERE GameID=? AND "
                               "ArtifactType='PBP' AND FormatKey='PFF_QUEUE_TEST_TEXT' AND "
                               "FormatVersion='1' AND HashAlgorithm='FNV1A64'", {game});
        require(artifact.row() && artifact.text() == relative.generic_u8string(),
                WorldEditError::IntegrityFailure, "Missing or invalid final play history");
        const auto bytes = admissionBytes(WorldEditingAccess::path(actor).parent_path() / relative);
        require(ClockTime::digest(bytes) == artifact.text(1), WorldEditError::IntegrityFailure,
                "Final play-history checksum mismatch");
        std::istringstream lines(bytes);
        result.log.clear();
        for (std::string line; std::getline(lines, line);)
            result.log.push_back(std::move(line));
    }
    else
        readTestPbp(s, actor, game);
    return result;
}
void validateTestSnapshot(Session &s, std::int64_t game, const WorldTestGameStep &tail,
                          const WorldTestSavedState &saved, bool final)
{
    try
    {
        WorldTestStateCodec::validate(saved);
    }
    catch (const std::exception &e)
    {
        throw Failure(WorldEditError::InvalidValue, e.what());
    }
    S::Query q(s.db,
               "SELECT r.InputBindingsHash FROM GameRuns r JOIN Games g ON g.WorldID=r.WorldID AND "
               "g.GameID=r.GameID WHERE r.WorldID=? AND r.GameID=? AND r.ProviderKey=? AND "
               "r.ProviderVersion='1' AND g.GameStatus='IN_PROGRESS'",
               {s.world, game, std::string(TestProvider)});
    require(q.row(), WorldEditError::UnsupportedProvider, "No matching saved-game provider");
    const auto &g = saved.game;
    require(saved.gameId == game && saved.inputHash == q.text() && g.generation == tail.generation &&
                g.playOrdinal == tail.playOrdinal && g.quarter == tail.period &&
                g.remaining == tail.remaining && g.elapsed == tail.elapsed && g.final == final &&
                tail.final == final && g.teams[0].score() == tail.awayScore &&
                g.teams[1].score() == tail.homeScore,
            WorldEditError::IntegrityFailure, "Runtime state does not match the saved step");
}
} // namespace

WorldEditResult<WorldEditCommit> saveTestMatchup(const WorldActor &actor, std::int64_t game,
                                                 const std::vector<WorldTestGameStep> &supplied,
                                                 const WorldTestSavedState &saved, std::int64_t expected,
                                                 int pauseThrough)
{
    return attempt<WorldEditCommit>([&] {
        const auto bytes = WorldTestStateCodec::encode(saved), hash = WorldStorageHash::bytes(bytes);
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
        S::Query run(s.db,
                     "SELECT r.WindowID,r.State,g.CompetitionSeasonID FROM GameRuns r JOIN Games g ON "
                     "g.WorldID=r.WorldID AND g.GameID=r.GameID WHERE r.WorldID=? AND r.GameID=? AND "
                     "g.GameStatus='IN_PROGRESS'",
                     {s.world, game});
        require(run.row(), WorldEditError::InvalidValue, "No unfinished game");
        const auto window = run.integer();
        const auto frozen = admissionInput(s, actor, game);
        admissionVerifyProvider(s, actor, run.integer(2), frozen[2]);
        S::Query latest(s.db,
                        "SELECT Generation,ContentHash,ClaimID FROM GameExecutionCheckpoints WHERE WorldID=? "
                        "AND GameID=? ORDER BY Generation DESC LIMIT 1",
                        {s.world, game});
        require(latest.row(), WorldEditError::IntegrityFailure, "Missing checkpoint");
        if (latest.integer() == saved.game.generation && latest.text(1) == hash)
        {
            auto result = testCommit(s);
            result.changed = false;
            result.replayed = true;
            result.committedRevision = S::scalar(
                s.db, "SELECT max(DataRevision) FROM WorldExecutionContexts WHERE WorldID=? AND ClaimID=?",
                {s.world, latest.integer(2)});
            tx.commit();
            return result;
        }
        require(s.revision == expected, WorldEditError::StaleRevision,
                "World changed; game remains in memory and has not been saved");
        require(
            run.text(1) == "RUNNING" && admissionActive(s) == window &&
                S::scalar(
                    s.db,
                    "SELECT count(*) FROM WorldSettings WHERE WorldID=? AND SettingKey=? AND SettingValue=?",
                    {s.world, admissionSelectionKey(window), std::to_string(game)}) == 1,
            WorldEditError::ControlConflict, "Game is not the active executor selection");
        std::vector<WorldTestGameStep> steps = supplied;
        if (steps.empty())
        {
            WorldTestGameStep p;
            p.generation = saved.game.generation;
            p.playOrdinal = saved.game.playOrdinal;
            p.period = saved.game.quarter;
            p.remaining = saved.game.remaining;
            p.elapsed = saved.game.elapsed;
            p.awayScore = saved.game.teams[0].score();
            p.homeScore = saved.game.teams[1].score();
            p.play = false;
            steps.push_back(p);
            require(p.generation == latest.integer() + 1, WorldEditError::StaleRevision,
                    "Empty save must create the next snapshot generation");
            if (latest.integer() > 0)
            {
                auto old = readTestProgress(s, actor, game), copy = saved;
                copy.game.generation = old.game.generation;
                require(WorldTestStateCodec::encode(old) == WorldTestStateCodec::encode(copy),
                        WorldEditError::IntegrityFailure, "Empty save changed runtime state");
            }
            else
            {
                std::mt19937 rng(static_cast<std::uint32_t>(std::stoull(frozen[3])));
                std::ostringstream out;
                out.imbue(std::locale::classic());
                out << rng;
                WorldTestSavedState initial;
                initial.gameId = game;
                initial.inputHash = frozen.empty() ? std::string{} : saved.inputHash;
                initial.game.generation = p.generation;
                initial.randomState = out.str();
                initial.log = {"=== GAME: " + frozen[10] + " at " + frozen[11] + " ===", "=== QUARTER 1 ==="};
                require(WorldTestStateCodec::encode(initial) == bytes, WorldEditError::IntegrityFailure,
                        "Initial save changed the admitted starting state");
            }
        }
        require(steps.size() <= 10000 && !steps.back().final, WorldEditError::InvalidValue,
                "Use finalization for a completed game");
        validateTestSnapshot(s, game, steps.back(), saved, false);
        if (pauseThrough >= 0)
        {
            require(pauseThrough >= saved.game.elapsed && pauseThrough <= 3600,
                    WorldEditError::InvalidValue, "Invalid interruption horizon");
            const auto boundary = testInterruption(s, game, pauseThrough);
            require(boundary && boundary->elapsedSeconds > saved.game.elapsed,
                    WorldEditError::StaleChoice, "Event changed; refresh the game before retrying");
        }
        std::string pbp = readTestPbp(s, actor, game);
        const auto rev = s.revision + 1;
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            require(!steps[i].final, WorldEditError::InvalidValue,
                    "Partial save cannot contain a final result");
            persistBufferedTestStep(s, game, steps[i], rev, {}, pbp, i + 1 == steps.size() ? &saved : nullptr,
                                    i + 1 == steps.size());
        }
        if (pauseThrough >= 0)
            S::run(s.db, "INSERT INTO WorldSettings(WorldID,SettingKey,SettingValue) VALUES(?,?,?)",
                   {s.world, testInterruptionKey(game), std::to_string(pauseThrough)});
        const auto path = admissionWrite(actor, pbp, "Progress-");
        S::run(s.db,
               "INSERT INTO "
               "GameArtifacts(GameID,ArtifactType,RelativePath,FormatKey,FormatVersion,HashAlgorithm,"
               "ContentHash,CreatedUtc) VALUES(?,?,?,'GAME_TIME_PBP_V1','1','SHA256',?,?)",
               {game, "PROGRESS_PBP:" + std::to_string(saved.game.generation),
                (std::filesystem::path("PBP") / path.filename()).generic_u8string(),
                WorldStorageHash::bytes(pbp), s.now});
        S::run(s.db, "UPDATE GameRuns SET State='PAUSED' WHERE WorldID=? AND GameID=? AND State='RUNNING'",
               {s.world, game});
        S::run(s.db,
               "UPDATE GameProcessingWindows SET State='PAUSED' WHERE WorldID=? AND WindowID=? AND "
               "State='ADMITTED'",
               {s.world, window});
        schedulerRevision(s, rev);
        auto result = testCommit(s);
        tx.commit();
        return result;
    });
}
