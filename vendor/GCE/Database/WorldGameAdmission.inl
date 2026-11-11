// WINDOW-ADMISSION-TASK2-2026-11-04. Shared admission for the installed,
// explicitly reduced GameTime executor. FULL_PFF capability is not implied.
#include "GCE/Database/WorldStorageHash.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace
{
void admissionPart(std::string &out, const std::string &value)
{
    out += std::to_string(value.size()) + ":" + value;
}
std::string admissionTake(const std::string &bytes, std::size_t &at)
{
    const auto colon = bytes.find(':', at);
    require(colon != std::string::npos && colon - at < 20, WorldEditError::IntegrityFailure,
            "Invalid admission input codec");
    const auto n = std::stoull(bytes.substr(at, colon - at));
    at = colon + 1;
    require(n <= bytes.size() - at, WorldEditError::IntegrityFailure, "Truncated admission input");
    auto value = bytes.substr(at, n);
    at += n;
    return value;
}
void admissionRows(Session &s, std::string &out, const std::string &label, const char *sql,
                   std::initializer_list<S::Value> args)
{
    admissionPart(out, label);
    S::Query q(s.db, sql, args);
    while (q.row())
        for (int c = 0; c < sqlite3_column_count(q.statement); ++c)
        {
            admissionPart(out, sqlite3_column_name(q.statement, c));
            admissionPart(out, std::to_string(sqlite3_column_type(q.statement, c)));
            admissionPart(out, q.text(c));
        }
    admissionPart(out, "END_" + label);
}
void admissionSafePath(const std::filesystem::path &path)
{
    const auto flags = GetFileAttributesW(path.c_str());
    require(flags == INVALID_FILE_ATTRIBUTES || !(flags & FILE_ATTRIBUTE_REPARSE_POINT),
            WorldEditError::IoError, "Admission artifacts cannot use junctions or symbolic links");
}
std::string admissionBytes(const std::filesystem::path &path)
{
    admissionSafePath(path);
    std::ifstream f(path, std::ios::binary);
    require(bool(f), WorldEditError::IoError, "Missing admission input artifact");
    require(std::filesystem::file_size(path) <= 64u * 1024u * 1024u, WorldEditError::IntegrityFailure,
            "Admission input is too large");
    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    require(!f.bad(), WorldEditError::IoError, "Cannot read admission input artifact");
    return bytes;
}
std::filesystem::path admissionWrite(const WorldActor &actor, const std::string &bytes,
                                     const std::string &prefix = "Input-")
{
    require(bytes.size() <= 64u * 1024u * 1024u, WorldEditError::UnsupportedFeature,
            "Admission input exceeds the installed codec's size capability");
    const auto root = WorldEditingAccess::path(actor).parent_path(), folder = root / "PBP";
    admissionSafePath(root);
    admissionSafePath(folder);
    std::filesystem::create_directories(folder);
    require(prefix == "Input-" || prefix == "Progress-", WorldEditError::InvalidValue,
            "Invalid artifact kind");
    const auto path = folder / (prefix + WorldStorageHash::bytes(bytes) + ".txt");
    if (std::filesystem::exists(path))
    {
        require(admissionBytes(path) == bytes, WorldEditError::IntegrityFailure,
                "Admission artifact hash conflict");
        return path;
    }
    HANDLE f =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(f != INVALID_HANDLE_VALUE, WorldEditError::IoError, "Cannot create admission artifact");
    std::size_t at = 0;
    bool good = true;
    while (good && at < bytes.size())
    {
        DWORD n = 0;
        good = WriteFile(f, bytes.data() + at, static_cast<DWORD>(bytes.size() - at), &n, nullptr) && n > 0;
        at += n;
    }
    good = good && FlushFileBuffers(f);
    CloseHandle(f);
    if (!good)
    {
        std::filesystem::remove(path);
        throw Failure(WorldEditError::IoError, "Cannot flush admission artifact");
    }
    // Unreferenced content-addressed inputs after rollback are harmless; they can
    // be reused on retry. Never remove an existing artifact or overwrite one.
    return path;
}
void admissionScope(Session &s, std::int64_t season)
{
    require(s.clockModel, WorldEditError::UnsupportedSchema,
            "Obsolete development schema; build a fresh World5 league");
    require(S::scalar(s.db,
                      "SELECT count(*) FROM CompetitionSeasons cs JOIN WorldCompetitions c ON "
                      "c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND "
                      "cs.CompetitionSeasonID=? AND cs.Status='ACTIVE' AND c.Active=1",
                      {s.world, season}) == 1,
            WorldEditError::InvalidValue, "No active season in this World");
}
std::string admissionNext(Session &s, std::int64_t season)
{
    S::Query q(
        s.db,
        "SELECT min(g.ScheduledUtc) FROM WorldScheduledGames g WHERE g.CompetitionSeasonID=? AND NOT "
        "EXISTS(SELECT 1 FROM GameRuns r JOIN GameFinalSaves f ON f.WorldID=r.WorldID AND f.GameID=r.GameID "
        "JOIN GameWindowMembers m ON m.WorldID=r.WorldID AND m.ScheduledWorldEventID=r.ScheduledWorldEventID "
        "WHERE r.WorldID=? AND m.WorldScheduledGameID=g.WorldScheduledGameID AND r.State='FINAL')",
        {season, s.world});
    q.row();
    return q.text();
}
std::int64_t admissionActive(Session &s)
{
    S::Query q(
        s.db, "SELECT WindowID FROM GameProcessingWindows WHERE WorldID=? AND State IN ('ADMITTED','PAUSED')",
        {s.world});
    return q.row() ? q.integer() : 0;
}
bool admissionEarlier(Session &s, const std::string &kickoff)
{
    return S::scalar(s.db,
                     "SELECT count(*) FROM WorldScheduledGames g JOIN CompetitionSeasons cs ON "
                     "cs.CompetitionSeasonID=g.CompetitionSeasonID JOIN WorldCompetitions c ON "
                     "c.WorldCompetitionID=cs.WorldCompetitionID WHERE c.WorldID=? AND g.ScheduledUtc<? AND "
                     "NOT EXISTS(SELECT 1 FROM GameWindowMembers m JOIN GameRuns r ON r.WorldID=m.WorldID "
                     "AND r.ScheduledWorldEventID=m.ScheduledWorldEventID JOIN GameFinalSaves f ON "
                     "f.WorldID=r.WorldID AND f.GameID=r.GameID WHERE m.WorldID=c.WorldID AND "
                     "m.WorldScheduledGameID=g.WorldScheduledGameID AND r.State='FINAL')",
                     {s.world, kickoff}) > 0;
}
void admissionCheckMembers(Session &s, std::int64_t season, const std::string &kickoff)
{
    const auto count = S::scalar(
        s.db, "SELECT count(*) FROM WorldScheduledGames WHERE CompetitionSeasonID=? AND ScheduledUtc=?",
        {season, kickoff});
    require(count > 0 && S::scalar(s.db,
                                   "SELECT count(*) FROM WorldScheduledGames g JOIN GameKickoffOccurrences k "
                                   "ON k.WorldScheduledGameID=g.WorldScheduledGameID AND k.WorldID=? AND "
                                   "k.IsCurrent=1 JOIN ScheduledWorldEvents e ON e.WorldID=k.WorldID AND "
                                   "e.ScheduledWorldEventID=k.ScheduledWorldEventID WHERE "
                                   "g.CompetitionSeasonID=? AND g.ScheduledUtc=? AND g.Status='SCHEDULED' "
                                   "AND e.StartsUtc=g.ScheduledUtc AND e.State='PENDING'",
                                   {s.world, season, kickoff}) == count,
            WorldEditError::UnsupportedFeature,
            "Canceled, replaced, or incomplete fixture cohort has no supported admission policy");
}
std::string admissionSelectionKey(std::int64_t window)
{
    return "Admission.Executing:" + std::to_string(window);
}
void admissionVerifyProvider(Session &s, const WorldActor &actor, std::int64_t season,
                             const std::string &evidence)
{
    require(evidence.rfind("Provider=PFF_GENERIC_QUEUE_TEST/2\nCoreSHA256=", 0) == 0,
            WorldEditError::UnsupportedProvider, "Prepare the installed GameTime executor before admission");
    const auto at = evidence.find("\nSeed=");
    require(at != std::string::npos, WorldEditError::InvalidValue, "Missing explicit provider seed");
    const auto end = evidence.find('\n', at + 6);
    const auto value = evidence.substr(at + 6, end - (at + 6));
    std::size_t n = 0;
    const auto seed = std::stoull(value, &n);
    require(n == value.size() && seed <= UINT32_MAX, WorldEditError::InvalidValue, "Invalid provider seed");
    S::Query q(s.db,
               "SELECT p.SnapshotPath,p.SnapshotFileHash FROM CompetitionSeasons c JOIN WorldCoreSnapshots p "
               "ON p.WorldCoreSnapshotID=c.WorldCoreSnapshotID AND p.WorldID=? WHERE c.CompetitionSeasonID=?",
               {s.world, season});
    require(q.row(), WorldEditError::IntegrityFailure, "Missing preserved Core");
    const auto relative = std::filesystem::u8path(q.text());
    require(!relative.is_absolute() && relative.parent_path() == "CoreSnapshots",
            WorldEditError::IntegrityFailure, "Unsafe Core snapshot path");
    const auto path = WorldEditingAccess::path(actor).parent_path() / relative;
    admissionSafePath(path.parent_path());
    admissionSafePath(path);
    require(evidence.find("CoreSHA256=" + q.text(1) + "\n") != std::string::npos &&
                WorldStorageHash::file(path) == q.text(1),
            WorldEditError::IntegrityFailure, "Provider/Core snapshot binding changed");
}
std::vector<std::string> admissionInput(Session &s, const WorldActor &actor, std::int64_t game)
{
    S::Query q(s.db,
               "SELECT a.RelativePath,a.ContentHash,r.InputBindingsHash FROM GameRuns r JOIN GameArtifacts a "
               "ON a.GameArtifactID=r.InputArtifactID AND a.GameID=r.GameID WHERE r.WorldID=? AND r.GameID=? "
               "AND a.FormatKey='WINDOW_INPUT_V1' AND a.HashAlgorithm='SHA256'",
               {s.world, game});
    require(q.row(), WorldEditError::UnsupportedFeature,
            "Obsolete development window inputs; create a fresh league");
    const auto relative = std::filesystem::u8path(q.text());
    require(!relative.is_absolute() && relative.parent_path() == "PBP" &&
                relative.filename() == ("Input-" + q.text(1) + ".txt"),
            WorldEditError::IntegrityFailure, "Unsafe admission artifact path");
    auto bytes = admissionBytes(WorldEditingAccess::path(actor).parent_path() / relative);
    require(WorldStorageHash::bytes(bytes) == q.text(1) && q.text(1) == q.text(2),
            WorldEditError::IntegrityFailure, "Admission input checksum mismatch");
    std::vector<std::string> fields;
    std::size_t at = 0;
    for (int n = 0; n < 17; ++n)
        fields.push_back(admissionTake(bytes, at));
    require(fields[0] == "WINDOW_INPUT_V1" && fields[1] == TestProvider, WorldEditError::UnsupportedProvider,
            "Unsupported admitted provider");
    return fields;
}
std::int64_t admitCohort(Session &s, const WorldActor &actor, std::int64_t season, std::int64_t request,
                         const std::string &command, const std::string &evidence)
{
    const auto kickoff = admissionNext(s, season);
    const auto rev = s.revision + 1;
    require(!kickoff.empty() && kickoff == s.now, WorldEditError::ClockBarrier,
            "Admission requires the exact earliest kickoff");
    admissionCheckMembers(s, season, kickoff);
    require(!admissionEarlier(s, kickoff), WorldEditError::EarlierGameUnresolved,
            "An earlier kickoff lacks durable final saves");
    require(evidence.rfind("Provider=PFF_GENERIC_QUEUE_TEST/2\nCoreSHA256=", 0) == 0,
            WorldEditError::UnsupportedProvider,
            "Prepare the installed GameTime provider inputs before admission");
    const auto seedAt = evidence.find("\nSeed=");
    require(seedAt != std::string::npos, WorldEditError::InvalidValue, "Missing explicit provider seed");
    const auto seedEnd = evidence.find('\n', seedAt + 6);
    const auto seedText = evidence.substr(seedAt + 6, seedEnd - (seedAt + 6));
    std::size_t used = 0;
    const auto seed = std::stoull(seedText, &used);
    require(used == seedText.size() && seed <= UINT32_MAX, WorldEditError::InvalidValue,
            "Invalid provider seed");
    S::Query core(s.db,
                  "SELECT p.SnapshotPath,p.SnapshotFileHash,p.SimulationRulesProfileKey FROM "
                  "CompetitionSeasons c JOIN WorldCoreSnapshots p ON "
                  "p.WorldCoreSnapshotID=c.WorldCoreSnapshotID AND p.WorldID=? WHERE c.CompetitionSeasonID=?",
                  {s.world, season});
    require(core.row(), WorldEditError::IntegrityFailure, "Missing preserved Core");
    const auto corePath = std::filesystem::u8path(core.text());
    require(!corePath.is_absolute() && corePath.parent_path() == "CoreSnapshots",
            WorldEditError::IntegrityFailure, "Unsafe Core snapshot path");
    const auto actual = WorldEditingAccess::path(actor).parent_path() / corePath;
    admissionSafePath(actual.parent_path());
    admissionSafePath(actual);
    require(WorldStorageHash::file(actual) == core.text(1) &&
                evidence.find("CoreSHA256=" + core.text(1) + "\n") != std::string::npos,
            WorldEditError::IntegrityFailure, "Provider/Core snapshot binding changed");
    require(!S::scalar(s.db, "SELECT count(*) FROM GameProcessingWindows WHERE WorldID=? AND State='DRAFT'",
                       {s.world}),
            WorldEditError::IntegrityFailure, "Incomplete window publication");
    S::Query fixtures(
        s.db,
        "SELECT "
        "g.WorldScheduledGameID,k.ScheduledWorldEventID,g.AwayTeamSeasonID,g.HomeTeamSeasonID,g.WorldVenueID,"
        "a.DisplayName,h.DisplayName,a.OriginCoreTeamSeasonKey,h.OriginCoreTeamSeasonKey,g.StageCode FROM "
        "WorldScheduledGames g JOIN GameKickoffOccurrences k ON "
        "k.WorldScheduledGameID=g.WorldScheduledGameID AND k.WorldID=? AND k.IsCurrent=1 JOIN "
        "ScheduledWorldEvents e ON e.WorldID=k.WorldID AND e.ScheduledWorldEventID=k.ScheduledWorldEventID "
        "JOIN TeamSeasons a ON a.TeamSeasonID=g.AwayTeamSeasonID AND "
        "a.CompetitionSeasonID=g.CompetitionSeasonID JOIN TeamSeasons h ON h.TeamSeasonID=g.HomeTeamSeasonID "
        "AND h.CompetitionSeasonID=g.CompetitionSeasonID WHERE g.CompetitionSeasonID=? AND g.ScheduledUtc=? "
        "AND e.State='PENDING' AND e.StartsUtc=g.ScheduledUtc AND a.Status='ACTIVE' AND h.Status='ACTIVE' "
        "ORDER BY g.WorldScheduledGameID",
        {s.world, season, kickoff});
    struct Fixture
    {
        std::int64_t id, event, away, home, venue;
        std::string awayName, homeName, awayKey, homeKey;
    };
    std::vector<Fixture> list;
    std::string membership;
    while (fixtures.row())
    {
        require(visibleEvent(s, fixtures.integer(1)), WorldEditError::NotAuthorized,
                "WAITING_FOR_WORLD_WORK");
        require(!clockAttention(s, fixtures.integer(1)), WorldEditError::DueWorkPending,
                "REQUIRED_ATTENTION");
        require(
            fixtures.text(9) != "SUPER_BOWL" ||
                !S::scalar(
                    s.db,
                    "SELECT count(*) FROM SeasonEventDefinitions WHERE WorldID=? AND CompetitionSeasonID=? "
                    "AND TriggerKind='AFTER_CHAMPIONSHIP_FINAL' AND InitializationMode='PENDING'",
                    {s.world, season}),
            WorldEditError::UnsupportedHandler,
            "Championship final-event production requires the later finalization service");
        list.push_back({fixtures.integer(), fixtures.integer(1), fixtures.integer(2), fixtures.integer(3),
                        fixtures.integer(4), fixtures.text(5), fixtures.text(6), fixtures.text(7),
                        fixtures.text(8)});
        admissionPart(membership, std::to_string(fixtures.integer()));
        admissionPart(membership, std::to_string(fixtures.integer(1)));
    }
    require(
        !list.empty() &&
            list.size() ==
                S::scalar(
                    s.db,
                    "SELECT count(*) FROM WorldScheduledGames WHERE CompetitionSeasonID=? AND ScheduledUtc=?",
                    {season, kickoff}),
        WorldEditError::IntegrityFailure,
        "Window requires every active fixture and exact occurrence binding");
    S::run(s.db,
           "INSERT OR IGNORE INTO GameClockProfiles "
           "VALUES(?,'PFF_QUEUE_TEST',2,4,900,'SUDDEN_DEATH','PFF_QUEUE_TEST_TIMING',1,'GAMETIME-TEST-OVERTIME-V2')",
           {s.world});
    require(
        S::scalar(s.db,
                  "SELECT count(*) FROM GameClockProfiles WHERE WorldID=? AND ProfileKey='PFF_QUEUE_TEST' "
                  "AND ProfileVersion=2 AND RegulationPeriods=4 AND QuarterSeconds=900 AND "
                  "OvertimePolicyKey='SUDDEN_DEATH' AND TimingPolicyKey='PFF_QUEUE_TEST_TIMING' AND "
                  "TimingPolicyVersion=1 AND PolicyContentHash='GAMETIME-TEST-OVERTIME-V2'",
                  {s.world}) == 1,
        WorldEditError::MissingClockProfile, "Installed test provider clock profile does not match");
    std::string common;
    admissionRows(s, common, "TEAMS",
                  "SELECT * FROM TeamSeasons WHERE CompetitionSeasonID=? ORDER BY TeamSeasonID", {season});
    admissionRows(s, common, "ROSTERS",
                  "SELECT r.* FROM RosterAssignments r JOIN TeamSeasons t ON t.TeamSeasonID=r.TeamSeasonID "
                  "WHERE t.CompetitionSeasonID=? AND (r.StartDateUtc IS NULL OR r.StartDateUtc<=?) AND "
                  "(r.EndDateUtc IS NULL OR r.EndDateUtc>?) ORDER BY r.RosterAssignmentID",
                  {season, kickoff, kickoff});
    admissionRows(s, common, "STAFF",
                  "SELECT * FROM RoleAssignments WHERE WorldID=? AND StartsUtc<=? AND (EndsUtc IS NULL OR "
                  "EndsUtc>?) ORDER BY RoleAssignmentID",
                  {s.world, kickoff, kickoff});
    admissionRows(s, common, "POSITIONS", "SELECT * FROM RolePositions WHERE WorldID=? ORDER BY PositionID",
                  {s.world});
    admissionRows(s, common, "PEOPLE", "SELECT * FROM People WHERE WorldID=? ORDER BY PersonID", {s.world});
    admissionRows(
        s, common, "MODIFIERS",
        "SELECT m.* FROM PlayerRatingModifiers m JOIN WorldPlayerSeasons p ON "
        "p.WorldPlayerSeasonID=m.WorldPlayerSeasonID WHERE p.CompetitionSeasonID=? AND (m.EffectiveUtc IS "
        "NULL OR m.EffectiveUtc<=?) AND (m.EndUtc IS NULL OR m.EndUtc>?) ORDER BY m.ModifierID",
        {season, kickoff, kickoff});
    S::run(s.db,
           "INSERT INTO "
           "GameProcessingWindows(WorldID,CompetitionSeasonID,WindowKey,KickoffUtc,MembershipHash,State,"
           "CreatedByUserID,CreatedRevision) VALUES(?,?,?,?,?,'DRAFT',?,?)",
           {s.world, season, "window:" + std::to_string(season) + ":" + kickoff, kickoff,
            WorldStorageHash::bytes(membership), s.user, rev});
    const auto window = S::inserted(s.db);
    for (const auto &f : list)
        S::run(s.db, "INSERT INTO GameWindowMembers VALUES(?,?,?,?)", {s.world, window, f.event, f.id});
    S::run(s.db, "UPDATE GameProcessingWindows SET State='ADMITTED' WHERE WorldID=? AND WindowID=?",
           {s.world, window});
    for (const auto &f : list)
    {
        const auto venue =
            f.venue ? ClockStateSql::venueAt(s.db, s.world, f.venue, kickoff) : ClockStateSql::Venue{};
        const auto gameSeed =
            static_cast<std::uint32_t>(seed) ^ static_cast<std::uint32_t>(f.id * 2654435761ULL);
        std::string input;
        for (const auto &field : std::vector<std::string>{
                 "WINDOW_INPUT_V1", TestProvider, evidence, std::to_string(gameSeed), core.text(1),
                 std::to_string(f.id), std::to_string(f.event), kickoff, std::to_string(f.away),
                 std::to_string(f.home), f.awayName, f.homeName, f.awayKey, f.homeKey,
                 std::to_string(venue.id), core.text(2), std::to_string(s.revision)})
            admissionPart(input, field);
        admissionPart(input, venue.name);
        admissionPart(input, venue.surface.value_or("UNKNOWN"));
        admissionPart(input, venue.hash);
        admissionPart(input, common);
        const auto hash = WorldStorageHash::bytes(input);
        const auto path = admissionWrite(actor, input);
        S::run(s.db,
               "INSERT INTO "
               "Games(GameKey,WorldID,CompetitionSeasonID,WorldScheduledGameID,AwayTeamSeasonID,"
               "HomeTeamSeasonID,WorldVenueID,GameStatus,ResolutionMode,SimulationProviderKey,"
               "SimulationProviderVersion,StartedUtc,AwayScore,HomeScore,ProviderCoverageKey) "
               "VALUES(?,?,?,?,?,?,?,'IN_PROGRESS','SIMULATED',?,'2',?,0,0,'TEAM_ONLY_TEST')",
               {"queue-test:" + std::to_string(f.id), s.world, season, f.id, f.away, f.home,
                f.venue ? S::Value(f.venue) : S::Value(nullptr), std::string(TestProvider), kickoff});
        const auto game = S::inserted(s.db);
        S::run(s.db,
               "INSERT INTO "
               "GameArtifacts(GameID,ArtifactType,RelativePath,FormatKey,FormatVersion,HashAlgorithm,"
               "ContentHash,CreatedUtc) VALUES(?,'INPUT',?,'WINDOW_INPUT_V1','1','SHA256',?,?)",
               {game, (std::filesystem::path("PBP") / path.filename()).generic_u8string(), hash, kickoff});
        const auto artifact = S::inserted(s.db);
        S::run(s.db, "INSERT INTO GameRuns VALUES(?,?,?,?,?,'PFF_QUEUE_TEST',2,?,'2',?,?,?,'PREPARED')",
               {s.world, game, window, f.event, kickoff, std::string(TestProvider), hash, artifact,
                venue.id ? S::Value(venue.id) : S::Value(nullptr)});
        for (std::int64_t period = 1; period <= 4; ++period)
            S::run(
                s.db,
                "INSERT INTO GamePeriods VALUES(?,?,?,'REGULATION','COUNTDOWN',900,'PFF_QUEUE_TEST_TIMING')",
                {s.world, game, period});
        const auto claim = testClaim(s, game, -1, 0, hash),
                   context = testContext(s, game, claim, kickoff, rev, "admission:" + std::to_string(game));
        S::run(s.db,
               "INSERT INTO "
               "GameVenueEvidence(GameID,WorldID,WorldVenueID,EvidenceKind,VenueName,TurfType,CapturedUtc,"
               "DataRevision,CaptureContextID) VALUES(?,?,?,?,?,?,?,?,?)",
               {game, s.world, f.venue ? S::Value(f.venue) : S::Value(nullptr),
                std::string(f.venue ? "CAPTURED" : "UNKNOWN_VENUE"),
                f.venue ? S::Value(venue.name) : S::Value(nullptr), S::nullable(venue.surface), kickoff, rev,
                context});
        if (f.venue)
            S::run(
                s.db,
                "INSERT INTO "
                "CausalReadFences(WorldID,ContextID,ResourceKind,ResourceKey,AsOfUtc,VersionHash,AsOfOrder) "
                "SELECT ?,?,'VENUE',?,?,?,OrderInInstant FROM WorldExecutionContexts WHERE ContextID=?",
                {s.world, context, std::to_string(f.venue), kickoff, venue.hash, context});
        S::run(s.db,
               "INSERT INTO "
               "CausalReadFences(WorldID,ContextID,ResourceKind,ResourceKey,AsOfUtc,VersionHash,AsOfOrder) "
               "SELECT ?,?,'GAME_INPUTS',?,?,?,OrderInInstant FROM WorldExecutionContexts WHERE ContextID=?",
               {s.world, context, std::to_string(game), kickoff, hash, context});
        WorldTestGameStep initial;
        initial.play = false;
        initial.text = "=== GAME: " + f.awayName + " at " + f.homeName +
                       " ===\nSimplified Generic queue test; saved progress supported.\n=== QUARTER 1 ===\n";
        testCheckpoint(s, game, claim, initial);
        S::run(s.db, "UPDATE GameRuns SET State='RUNNING' WHERE WorldID=? AND GameID=?", {s.world, game});
        S::run(s.db,
               "UPDATE ScheduledWorldEvents SET State='RUNNING',StartedUtc=StartsUtc WHERE WorldID=? AND "
               "ScheduledWorldEventID=?",
               {s.world, f.event});
        S::run(s.db, "INSERT INTO RunningGameEvents VALUES(?,?,?)", {s.world, f.event, game});
        S::run(s.db, "INSERT INTO SchedulerDispatchReceipts VALUES(?,?,'START',?,?,?,?)",
               {s.world, f.event, context, hash, kickoff, rev});
    }
    require(S::scalar(s.db,
                      "SELECT count(*) FROM GameRuns WHERE WorldID=? AND WindowID=? AND State='RUNNING'",
                      {s.world, window}) == static_cast<std::int64_t>(list.size()),
            WorldEditError::IntegrityFailure, "Incomplete prepared cohort");
    S::run(s.db, "INSERT INTO WorldSettings VALUES(?,?,?)",
           {s.world, "Admission.Request:" + std::to_string(request), std::to_string(window)});
    S::run(s.db, "INSERT INTO WorldSettings VALUES(?,?,?)",
           {s.world, "Admission.Command:" + std::to_string(s.user) + ":" + command,
            std::to_string(request) + ":" + WorldStorageHash::bytes(std::to_string(season) + evidence)});
    advanceState(s, request, "COMPLETED", "WINDOW_ADMITTED", 0, 3, rev);
    schedulerRevision(s, rev);
    return window;
}
} // namespace

WorldGameWindow previewInstalledWindow(Session &s, std::int64_t season)
{
    admissionScope(s, season);
    WorldGameWindow r;
    r.worldId = s.world;
    S::Query identity(s.db, "SELECT WorldKey FROM Worlds WHERE WorldID=?", {s.world});
    identity.row();
    r.worldKey = identity.text();
    r.revision = s.revision;
    r.currentTimeUtc = s.now;
    r.windowId = admissionActive(s);
    r.kickoffUtc = admissionNext(s, season);
    if (r.windowId)
    {
        S::Query w(
            s.db,
            "SELECT CompetitionSeasonID,KickoffUtc FROM GameProcessingWindows WHERE WorldID=? AND WindowID=?",
            {s.world, r.windowId});
        w.row();
        r.kickoffUtc = w.text(1);
        if (w.integer() != season)
        {
            r.reason = "WINDOW_IN_PROGRESS";
            return r;
        }
    }
    r.games = S::scalar(s.db,
                        "SELECT count(*) FROM WorldScheduledGames g WHERE g.CompetitionSeasonID=? AND "
                        "g.ScheduledUtc=? AND NOT EXISTS(SELECT 1 FROM FinalizedLeagueGameResults f WHERE "
                        "f.WorldID=? AND f.WorldScheduledGameID=g.WorldScheduledGameID)",
                        {season, r.kickoffUtc, s.world});
    if (!canAdvance(s))
        r.reason = "NOT_AUTHORIZED";
    else if (r.kickoffUtc.empty())
        r.reason = "NO_MORE_GAMES";
    else if (r.kickoffUtc < s.now || (!r.windowId && admissionEarlier(s, r.kickoffUtc)))
        r.reason = "EARLIER_GAME_UNRESOLVED";
    else if (r.windowId &&
             S::scalar(s.db, "SELECT count(*) FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                       {s.world, admissionSelectionKey(r.windowId)}))
    {
        S::Query saved(s.db, "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                       {s.world, admissionSelectionKey(r.windowId)});
        saved.row();
        r.hasSelectedGame = true;
        r.canStart = savedTestGameReady(s, std::stoll(saved.text()));
        r.reason = r.canStart ? "Resume saved matchup."
                              : "Game is already open or its development checkpoint is incompatible.";
    }
    else
    {
        r.canStart = true;
        r.reason = r.windowId ? "Continue the next admitted matchup in this window."
                              : "Admit the next kickoff window and play its first matchup.";
    }
    S::Query active(s.db,
                    "SELECT AdvanceRequestID,CommandKey,TargetUtc,StopReason FROM WorldAdvanceRequests WHERE WorldID=? "
                    "AND State IN ('RUNNING','PAUSED','BLOCKED')",
                    {s.world});
    if (active.row())
    {
        r.canStart = false;
        const auto prefix = "game-window:" + std::to_string(season) + ":";
        if (active.text(1).rfind(prefix, 0) == 0)
        {
            r.advance = advanceOutcome(s, active.integer());
            r.canResume = r.advance->state == "RUNNING";
            r.canStart = r.advance->reason == "GAME_WINDOW_ADMISSION_REQUIRED" ||
                         active.text(3) == "UNSUPPORTED_EVENT" ||
                         s.revision > r.advance->outcomeRevision;
            r.reason = r.advance->reason;
        }
        else
            r.reason = "ADVANCE_ALREADY_ACTIVE";
    }
    r.canStart = r.canStart && canAdvance(s);
    r.canResume = r.canResume && canAdvance(s);
    S::Query disclosure(s.db,
                        "SELECT k.ScheduledWorldEventID FROM GameKickoffOccurrences k JOIN "
                        "WorldScheduledGames g ON g.WorldScheduledGameID=k.WorldScheduledGameID WHERE "
                        "k.WorldID=? AND k.IsCurrent=1 AND g.CompetitionSeasonID=? AND g.ScheduledUtc=?",
                        {s.world, season, r.kickoffUtc});
    while (disclosure.row())
        if (!visibleEvent(s, disclosure.integer()))
        {
            r.canStart = false;
            r.canResume = false;
            r.hasSelectedGame = false;
            r.advance.reset();
            r.windowId = 0;
            r.games = 0;
            r.kickoffUtc.clear();
            r.reason = "WAITING_FOR_WORLD_WORK";
            break;
        }
    return r;
}

WorldEditResult<WorldAdvanceOutcome> beginInstalledGameWindow(const WorldActor &actor, std::int64_t season,
                                                              const std::string &key, std::int64_t expected,
                                                              const std::string &evidence)
{
    return attempt<WorldAdvanceOutcome>([&] {
        const auto command = text(key, 80, true);
        std::int64_t request = 0;
        std::string kickoff;
        {
            Session s(actor, false);
            admissionScope(s, season);
            require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
            S::Query replay(s.db, "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                            {s.world, "Admission.Command:" + std::to_string(s.user) + ":" + command});
            if (replay.row())
            {
                const auto value = replay.text();
                const auto colon = value.find(':');
                require(colon != std::string::npos &&
                            value.substr(colon + 1) ==
                                WorldStorageHash::bytes(std::to_string(season) + evidence),
                        WorldEditError::IdempotencyConflict, "Admission key has different provider inputs");
                auto r = advanceOutcome(s, std::stoll(value.substr(0, colon)));
                r.replayed = true;
                S::exec(s.db, "COMMIT");
                return r;
            }
            require(s.revision == expected, WorldEditError::StaleRevision,
                    "World changed; reload admission state");
            if (const auto window = admissionActive(s))
            {
                WorldAdvanceOutcome r;
                r.windowId = window;
                r.revision = s.revision;
                r.state = "BLOCKED";
                r.reason = "WINDOW_IN_PROGRESS";
                r.currentTimeUtc = s.now;
                S::exec(s.db, "COMMIT");
                return r;
            }
            admissionVerifyProvider(s, actor, season, evidence);
            kickoff = admissionNext(s, season);
            require(!kickoff.empty(), WorldEditError::InvalidValue, "No more scheduled games");
            admissionCheckMembers(s, season, kickoff);
            require(!admissionEarlier(s, kickoff), WorldEditError::EarlierGameUnresolved,
                    "An earlier kickoff lacks durable final saves");
            S::Query active(
                s.db,
                "SELECT AdvanceRequestID,CommandKey,TargetUtc,StartedByUserID FROM WorldAdvanceRequests "
                "WHERE WorldID=? AND State IN ('RUNNING','PAUSED','BLOCKED')",
                {s.world});
            if (active.row())
            {
                require(active.text(1).rfind("game-window:" + std::to_string(season) + ":", 0) == 0 &&
                            active.text(2) == kickoff && active.integer(3) == s.user,
                        WorldEditError::AdvanceAlreadyActive, "Resolve the existing World advancement first");
                request = active.integer();
            }
            S::exec(s.db, "COMMIT");
        }
        WorldAdvanceOutcome out;
        if (request)
        {
            auto r = loadWorldAdvance(actor, request);
            require(r.success, r.error, r.message.c_str());
            out = r.value;
        }
        else
        {
            auto r = beginWorldAdvance(actor, "game-window:" + std::to_string(season) + ":" + command,
                                       WorldAdvanceMode::ToTime, kickoff, expected);
            require(r.success, r.error, r.message.c_str());
            out = r.value;
        }
        {
            // Owner-approved GameTime scope: undated unrelated templates remain
            // unexecuted. This narrow request policy never skips DATED work and
            // never grants FULL_PFF readiness or affects ordinary World intents.
            Session s(actor, true);
            S::Transaction tx(s.db);
            s.refresh();
            require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
            require(s.revision == out.revision, WorldEditError::StaleRevision,
                    "World changed before admission policy");
            const auto scope = "Admission.Scope:" + std::to_string(out.requestId);
            if (!S::scalar(s.db, "SELECT count(*) FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                           {s.world, scope}))
            {
                S::run(s.db, "INSERT INTO WorldSettings VALUES(?,?,'GAME_TIME_REDUCED_V1')",
                       {s.world, scope});
                schedulerRevision(s, s.revision + 1);
            }
            out = advanceOutcome(s, out.requestId);
            tx.commit();
        }
        std::size_t units = 0;
        do
        {
            auto r = continueWorldAdvance(actor, out.requestId, out.revision, 64 - units);
            require(r.success, r.error, r.message.c_str());
            out = r.value;
            units += out.units;
        } while (out.state == "RUNNING" && units < 64 && !out.stale);
        if (out.stale || out.reason != "GAME_WINDOW_ADMISSION_REQUIRED")
            return out;
        Session s(actor, true);
        S::Transaction tx(s.db);
        s.refresh();
        admissionScope(s, season);
        require(canAdvance(s), WorldEditError::NotAuthorized, "World advancement permission required");
        require(s.revision == out.revision, WorldEditError::StaleRevision,
                "World changed before cohort commit");
        const auto window = admitCohort(s, actor, season, out.requestId, command, evidence);
        auto result = advanceOutcome(s, out.requestId);
        result.windowId = window;
        result.units = units + 1;
        tx.commit();
        return result;
    });
}
