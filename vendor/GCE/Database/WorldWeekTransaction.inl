// Implementation lives with the trusted editing session helpers.
namespace WorldWeekInternal
{
    void recordFinalizedResult(sqlite3* db, std::int64_t world, std::int64_t game, std::int64_t rev,
                               const std::string& now, const std::string& key, const std::string& fingerprint,
                               WorldEditCommit& result)
    {
        try
        {
            require(db && sqlite3_get_autocommit(db) == 0 &&
                        sqlite3_txn_state(db, "main") == SQLITE_TXN_WRITE,
                    WorldEditError::InvalidValue,
                    "Week participant requires the finalizer's active write transaction");
            require(S::utc(now), WorldEditError::InvalidValue, "Invalid completion simulation time");
            const auto command = text(key, 128, true), digest = text(fingerprint, 8192, true);
            require(S::scalar(db,
                              "SELECT count(*) FROM WorldSchemaInfo WHERE SchemaInfoID=1 AND SchemaVersion=4 "
                              "AND SchemaFamily='GCE_WORLD'") == 1,
                    WorldEditError::UnsupportedSchema, "Unsupported World contract");
            require(worldEventContractsReady(db, world),
                    WorldEditError::UnsupportedSchema, "Unsupported event/week feature contracts");
            S::Query identity(
                db, "SELECT WorldKey,CurrentTimeUtc FROM Worlds WHERE WorldID=? AND WorldStatus='ACTIVE'",
                {world});
            require(identity.row(), WorldEditError::InvalidValue, "Inactive World");
            result.worldId = world;
            result.worldKey = identity.text();
            result.currentTimeUtc = identity.text(1);
            S::Query replay(
                db,
                "SELECT GameID,ResultFingerprint,FinalizedRevision,CompletedUtc,FinalizationKey FROM "
                "FinalizedLeagueGameResults WHERE WorldID=? AND (GameID=? OR FinalizationKey=?)",
                {world, game, command});
            if (replay.row())
            {
                require(replay.integer() == game && replay.text(1) == digest && replay.text(4) == command,
                        WorldEditError::IdempotencyConflict,
                        "Finalization key or game already has a different committed result");
                result.committedRevision = replay.integer(2);
                result.revision = S::scalar(db, "SELECT Revision FROM WorldDataRevisions WHERE WorldID=?", {world});
                // Keep actual current World time from identity, not original receipt time.
                result.changed = false;
                result.replayed = true;
                require(!replay.row(), WorldEditError::IdempotencyConflict,
                        "Conflicting finalization receipts");
                return;
            }
            require(identity.text(1) == now, WorldEditError::GameNotFinalizable,
                    "Finalization must occur at the current simulation instant");
            const auto previous =
                S::scalar(db, "SELECT Revision FROM WorldDataRevisions WHERE WorldID=?", {world});
            require(previous < std::numeric_limits<std::int64_t>::max() && rev == previous + 1,
                    WorldEditError::StaleRevision, "Finalizer must share the current World revision");
            S::Query q(
                db,
                "SELECT "
                "g.WorldScheduledGameID,g.CompetitionSeasonID,g.CompletedUtc,g.StartedUtc,g.GameStatus,g."
                "AwayTeamSeasonID,g.HomeTeamSeasonID,g.AwayScore,g.HomeScore,g.WinnerTeamSeasonID,m."
                "LeagueWeekID,"
                "w.State,w.NextLeagueWeekID,sg.ScheduledUtc,g.WorldVenueID,sg.AwayTeamSeasonID,sg."
                "HomeTeamSeasonID FROM Games g JOIN WorldScheduledGames sg ON "
                "sg.WorldScheduledGameID=g.WorldScheduledGameID AND "
                "sg.CompetitionSeasonID=g.CompetitionSeasonID "
                "JOIN LeagueWeekGames m ON m.WorldID=g.WorldID AND "
                "m.WorldScheduledGameID=g.WorldScheduledGameID "
                "JOIN LeagueWeeks w ON w.WorldID=m.WorldID AND w.LeagueWeekID=m.LeagueWeekID WHERE "
                "g.WorldID=? "
                "AND g.GameID=?",
                {world, game});
            require(q.row(), WorldEditError::WeekStateUnavailable,
                    "Game has no supported required-week membership");
            const auto scheduled = q.integer(), season = q.integer(1), week = q.integer(10),
                       next = q.integer(12);
            require(q.text(4) == "FINAL" && q.text(11) == "OPEN" && q.text(2) == now && S::utc(q.text(3)) &&
                        q.text(3) <= now && q.text(3) >= q.text(13),
                    WorldEditError::GameNotFinalizable,
                    "Game must be final in the open week at its actual completion time");
            require(q.integer(5) == q.integer(15) && q.integer(6) == q.integer(16) && !q.null(7) &&
                        !q.null(8) && q.integer(7) >= 0 && q.integer(8) >= 0,
                    WorldEditError::GameNotFinalizable, "Final game teams or scores are invalid");
            const auto winner = q.integer(7) > q.integer(8)   ? q.integer(5)
                                : q.integer(8) > q.integer(7) ? q.integer(6)
                                                              : 0;
            require((winner == 0 && q.null(9)) || (winner != 0 && winner == q.integer(9)),
                    WorldEditError::GameNotFinalizable, "Winner does not match final scores");
            require(
                S::scalar(
                    db,
                    "SELECT count(*) FROM GameVenueEvidence WHERE WorldID=? AND GameID=? AND CapturedUtc=?",
                    {world, game, q.text(3)}) == 1,
                WorldEditError::GameNotFinalizable, "Game-start venue evidence is required");
            require(
                S::scalar(
                    db,
                    "SELECT count(*) FROM LeagueWeekGames m JOIN WorldScheduledGames s ON "
                    "s.WorldScheduledGameID=m.WorldScheduledGameID WHERE m.WorldID=? AND s.ScheduledUtc<? "
                    "AND NOT EXISTS(SELECT 1 FROM FinalizedLeagueGameResults f WHERE f.WorldID=m.WorldID AND "
                    "f.WorldScheduledGameID=m.WorldScheduledGameID) AND NOT EXISTS(SELECT 1 FROM Games live "
                    "WHERE live.WorldID=m.WorldID AND live.WorldScheduledGameID=m.WorldScheduledGameID AND "
                    "live.GameStatus='IN_PROGRESS' AND live.StartedUtc=s.ScheduledUtc)",
                    {world, q.text(13)}) == 0,
                WorldEditError::EarlierGameUnresolved,
                "An earlier kickoff has not been resolved or started by the coordinator");
            S::run(db, "INSERT INTO FinalizedLeagueGameResults VALUES(?,?,?,?,?,?,?)",
                   {world, scheduled, game, command, digest, now, rev});
            result.changed = true;
            result.revision = rev;
            result.committedRevision = rev;
            result.currentTimeUtc = now;
            result.teamSeasons.push_back(q.integer(5));
            result.teamSeasons.push_back(q.integer(6));
            if (S::scalar(
                    db,
                    "SELECT count(*) FROM LeagueWeekGames m WHERE m.WorldID=? AND m.LeagueWeekID=? AND NOT "
                    "EXISTS(SELECT 1 FROM FinalizedLeagueGameResults f WHERE f.WorldID=m.WorldID AND "
                    "f.WorldScheduledGameID=m.WorldScheduledGameID)",
                    {world, week}) != 0)
                return;
            S::run(db,
                   "UPDATE LeagueWeeks SET State='CLOSED',ClosedUtc=?,ClosedRevision=? WHERE WorldID=? AND "
                   "LeagueWeekID=? AND State='OPEN'",
                   {now, rev, world, week});
            require(sqlite3_changes(db) == 1, WorldEditError::IntegrityFailure,
                    "Current week changed during finalization");
            S::run(db,
                   "INSERT INTO "
                   "LeagueWeekTransitions(WorldID,CompetitionSeasonID,FromLeagueWeekID,ToLeagueWeekID,"
                   "TriggerGameID,OccurredUtc,DataRevision) VALUES(?,?,?,?,?,?,?)",
                   {world, season, week, next ? S::Value(next) : S::Value(nullptr), game, now, rev});
            if (!next)
                return; // Supported phase complete; no invented week or season.
            // Venue work is dispatched by its exact timestamp, never week closure.
            S::run(db,
                   "UPDATE LeagueWeeks SET State='OPEN',OpenedUtc=?,OpenedRevision=? WHERE WorldID=? AND "
                   "CompetitionSeasonID=? AND LeagueWeekID=? AND State='FUTURE'",
                   {now, rev, world, season, next});
            require(sqlite3_changes(db) == 1, WorldEditError::IntegrityFailure, "Next week is not available");
        }
        catch (const Failure& e)
        {
            throw TransactionError(e.code, e.what());
        }
        catch (const S::Error& e)
        {
            const auto code = e.code & 255;
            const bool io = code == SQLITE_IOERR || code == SQLITE_CANTOPEN || code == SQLITE_FULL ||
                            code == SQLITE_READONLY || code == SQLITE_BUSY || code == SQLITE_LOCKED;
            throw TransactionError(
                io ? WorldEditError::IoError : WorldEditError::EventApplicationFailed,
                "Result/week/event transaction failed; the finalizer must roll back before retrying");
        }
    }
} // namespace WorldWeekInternal
