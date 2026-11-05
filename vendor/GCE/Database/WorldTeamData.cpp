#include "GCE/Database/WorldTeamData.h"
#include "GCE/Database/WorldScheduler.h"
#include "GCE/Database/WorldDatabase.h"
#include "GCE/Database/WorldWeekTransaction.h"
#include "GCE/Database/TeamDataSql.h"
#include "GCE/Database/WorldEventReadiness.h"
#include "GCE/Database/WorldClockServices.h"
#include "GCE/Database/WorldTestGame.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <random>
#include <set>
#include <sstream>

struct WorldEditingAccess
{
    static sqlite3* handle(Database& d)
    {
        return d.db_;
    }
    static const std::filesystem::path& path(const WorldActor& a)
    {
        return a.database_;
    }
    static std::int64_t world(const WorldActor& a)
    {
        return a.world_;
    }
    static std::int64_t user(const WorldActor& a)
    {
        return a.user_;
    }
    static const std::string& worldKey(const WorldActor& a)
    {
        return a.worldKey_;
    }
    static const std::string& userKey(const WorldActor& a)
    {
        return a.userKey_;
    }
};
namespace
{
    namespace S = TeamDataSql;
    struct Failure : std::runtime_error
    {
        WorldEditError code;
        Failure(WorldEditError c, const std::string& m) : std::runtime_error(m), code(c)
        {
        }
    };
    void require(bool ok, WorldEditError code, const char* message)
    {
        if (!ok)
            throw Failure(code, message);
    }
    template <class T, class F> WorldEditResult<T> attempt(F&& f)
    {
        WorldEditResult<T> r;
        try
        {
            r.value = f();
            r.success = true;
        }
        catch (const Failure& e)
        {
            r.error = e.code;
            r.message = e.what();
            if(r.message.find("CAUSAL_FENCE")!=std::string::npos) r.error=WorldEditError::CausalFence;
        }
        catch (const S::Error& e)
        {
            const auto code = e.code & 255;
            r.error = (code == SQLITE_IOERR || code == SQLITE_CANTOPEN || code == SQLITE_FULL ||
                       code == SQLITE_READONLY || code == SQLITE_BUSY || code == SQLITE_LOCKED)
                          ? WorldEditError::IoError
                          : WorldEditError::IntegrityFailure;
            r.message = e.what();
            if(r.message.find("CAUSAL_FENCE")!=std::string::npos) r.error=WorldEditError::CausalFence;
        }
        catch (const std::exception& e)
        {
            r.error = WorldEditError::IntegrityFailure;
            r.message = e.what();
        }
        return r;
    }
    struct Session
    {
        Database connection;
        sqlite3* db = nullptr;
        std::int64_t world = 0, user = 0, revision = 0;
        std::int64_t context = 0, contextOrder = 0;
        bool clockModel = false;
        bool manage = false, enableGod = false, god = false;
        std::string now;
        Session(const WorldActor& actor, bool write)
        {
            require(!WorldEditingAccess::path(actor).empty(), WorldEditError::NotAuthorized,
                    "No trusted World session");
            require(connection.open(WorldEditingAccess::path(actor),
                                    write ? DatabaseOpenMode::ReadWrite : DatabaseOpenMode::ReadOnly),
                    WorldEditError::IoError, "Cannot open existing World database");
            db = WorldEditingAccess::handle(connection);
            sqlite3_busy_timeout(db, 3000);
            if (!write)
                S::exec(db, "BEGIN");
            require(
                S::scalar(
                    db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='WorldSchemaInfo'") ==
                    1,
                WorldEditError::UnsupportedSchema, "UNSUPPORTED_SCHEMA: create a fresh compatible World");
            require(S::scalar(db, "SELECT count(*) FROM WorldSchemaInfo WHERE SchemaInfoID=1 AND "
                                  "SchemaFamily='GCE_WORLD' AND SchemaVersion IN (4,5)") == 1,
                    WorldEditError::UnsupportedSchema,
                    "UNSUPPORTED_SCHEMA: create a fresh compatible World; existing saves are not migrated");
            require(S::scalar(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND "
                                  "name='WorldFeatureVersions'") == 1,
                    WorldEditError::UnsupportedSchema, "Unsupported venue/week feature contracts");
            world = WorldEditingAccess::world(actor);
            clockModel = ClockStateSql::enabled(db);
            user = WorldEditingAccess::user(actor);
            require(worldEventContractsReady(db, world),
                    WorldEditError::UnsupportedSchema, "Unsupported venue/week feature contracts");
            require(worldEventPublicationReady(db, world), WorldEditError::IntegrityFailure,
                    "Incomplete dated-event publication");
            S::Query q(
                db,
                "SELECT w.CurrentTimeUtc,r.Revision,u.CanManageTeams,u.CanEnableGodMode,m.GodModeEnabled "
                "FROM Worlds w JOIN WorldDataRevisions r ON r.WorldID=w.WorldID JOIN WorldUsers u ON "
                "u.WorldID=w.WorldID JOIN WorldUserEditModes m ON m.WorldID=u.WorldID AND "
                "m.WorldUserID=u.WorldUserID WHERE w.WorldID=? AND w.WorldKey=? AND u.WorldUserID=? AND "
                "u.UserKey=? AND u.Active=1 AND w.WorldStatus='ACTIVE'",
                {world, WorldEditingAccess::worldKey(actor), user, WorldEditingAccess::userKey(actor)});
            require(q.row(), WorldEditError::NotAuthorized, "World session identity is no longer available");
            now = q.text(0);
            require(S::utc(now),WorldEditError::IntegrityFailure,"Invalid World calendar clock");
            revision = q.integer(1);
            manage = q.integer(2) != 0;
            enableGod = q.integer(3) != 0;
            god = q.integer(4) != 0 && enableGod;
            require(S::utc(now), WorldEditError::IntegrityFailure, "Invalid canonical World clock");
            require(S::scalar(db, "SELECT count(*) FROM Worlds") == 1, WorldEditError::IntegrityFailure,
                    "World must contain exactly one identity");
        }
        void refresh()
        {
            S::Query q(
                db,
                "SELECT w.CurrentTimeUtc,r.Revision,u.CanManageTeams,u.CanEnableGodMode,m.GodModeEnabled "
                "FROM Worlds w JOIN WorldDataRevisions r ON r.WorldID=w.WorldID JOIN WorldUsers u ON "
                "u.WorldID=w.WorldID JOIN WorldUserEditModes m ON m.WorldID=u.WorldID AND "
                "m.WorldUserID=u.WorldUserID WHERE w.WorldID=? AND u.WorldUserID=? AND u.Active=1",
                {world, user});
            require(q.row(), WorldEditError::NotAuthorized, "Inactive user");
            now = q.text();
            revision = q.integer(1);
            manage = q.integer(2) != 0;
            enableGod = q.integer(3) != 0;
            god = q.integer(4) != 0 && enableGod;
        }
    };
    struct Team
    {
        std::int64_t id = 0, org = 0, competition = 0, venue = 0, season = 0;
        std::string city, nickname, name, venueName;
        std::optional<std::string> surface;
    };
    Team team(Session& s, std::int64_t id)
    {
        S::Query q(
            s.db,
            "SELECT "
            "t.TeamSeasonID,t.WorldOrganizationID,c.WorldCompetitionID,t.WorldVenueID,t.MarketName,t."
            "Nickname,t."
            "DisplayName,v.VenueName,v.TurfType,t.CompetitionSeasonID FROM TeamSeasons t JOIN "
            "WorldOrganizations o ON "
            "o.WorldOrganizationID=t.WorldOrganizationID JOIN CompetitionSeasons c ON "
            "c.CompetitionSeasonID=t.CompetitionSeasonID JOIN WorldCompetitions wc ON "
            "wc.WorldCompetitionID=c.WorldCompetitionID LEFT JOIN WorldVenues v ON "
            "v.WorldVenueID=t.WorldVenueID "
            "AND v.WorldID=o.WorldID WHERE t.TeamSeasonID=? AND o.WorldID=? AND wc.WorldID=o.WorldID AND "
            "t.Status='ACTIVE' AND c.Status='ACTIVE' AND o.Active=1 AND wc.Active=1",
            {id, s.world});
        require(q.row(), WorldEditError::TeamNotActive, "Select an active team in this World");
        Team t;
        t.id = q.integer();
        t.org = q.integer(1);
        t.competition = q.integer(2);
        t.venue = q.integer(3);
        t.city = q.text(4);
        t.nickname = q.text(5);
        t.name = q.text(6);
        t.venueName = q.text(7);
        t.surface = q.optionalText(8);
        t.season = q.integer(9);
        if(s.clockModel && t.venue)
        {
            const auto v=ClockStateSql::venueAt(s.db,s.world,t.venue,s.now);
            t.venueName=v.name;t.surface=v.surface;
        }
        return t;
    }
    bool capability(Session& s, const Team& t, const std::string& cap, std::int64_t targetRole = 0,
                    std::optional<bool> preview = {})
    {
        if (s.god)
            return true;
        const bool current =
            S::scalar(s.db, R"SQL(SELECT count(*) FROM CharacterRoleControls g
 JOIN Characters ch ON ch.CharacterID=g.CharacterID AND ch.WorldID=g.WorldID AND ch.EndsUtc IS NULL
 JOIN RoleAssignments a ON a.RoleAssignmentID=g.RoleAssignmentID AND a.WorldID=g.WorldID AND a.EndsUtc IS NULL
 JOIN RolePositions p ON p.PositionID=a.PositionID AND p.WorldID=g.WorldID AND p.Active=1
 JOIN AuthorityProfileCapabilities c ON c.AuthorityProfileID=p.AuthorityProfileID AND c.WorldID=p.WorldID
 WHERE g.WorldID=? AND g.WorldUserID=? AND g.EndsUtc IS NULL AND c.CapabilityKey=?
 AND (p.ScopeKind='WORLD' OR (p.ScopeKind='ORGANIZATION' AND p.WorldOrganizationID=?) OR (p.ScopeKind='COMPETITION' AND p.WorldCompetitionID=?))
 AND (p.TeamSeasonID IS NULL OR p.TeamSeasonID=?)
 AND (?=0 OR EXISTS(SELECT 1 FROM AuthorityProfileStaffTargets x WHERE x.WorldID=p.WorldID AND x.AuthorityProfileID=p.AuthorityProfileID AND x.TargetRoleID=?))
 AND (g.OriginKind='MANUAL' OR EXISTS(SELECT 1 FROM TeamManagementIntents i WHERE i.TeamManagementID=g.TeamManagementID AND i.EndsUtc IS NULL))
 AND (?=0 OR g.TeamManagementID IS NULL OR NOT EXISTS(SELECT 1 FROM TeamManagementIntents i WHERE i.TeamManagementID=g.TeamManagementID AND i.WorldOrganizationID=?)))SQL",
                      {s.world, s.user, cap, t.org, t.competition, t.id, targetRole, targetRole,
                       static_cast<std::int64_t>(preview && !*preview), t.org}) > 0;
        if (current || !preview || !*preview)
            return current;
        return S::scalar(s.db, R"SQL(SELECT count(*) FROM RolePositions p
 JOIN RoleDefinitions r ON r.WorldID=p.WorldID AND r.RoleID=p.RoleID
 JOIN RoleAssignments a ON a.WorldID=p.WorldID AND a.PositionID=p.PositionID AND a.EndsUtc IS NULL
 JOIN AuthorityProfileCapabilities c ON c.WorldID=p.WorldID AND c.AuthorityProfileID=p.AuthorityProfileID
 WHERE p.WorldID=? AND p.WorldOrganizationID=? AND p.Active=1 AND (p.TeamSeasonID IS NULL OR p.TeamSeasonID=?)
 AND r.RoleKey IN ('OWNER','GENERAL_MANAGER','HEAD_COACH','OFFENSIVE_COORDINATOR','DEFENSIVE_COORDINATOR')
 AND c.CapabilityKey=? AND (?=0 OR EXISTS(SELECT 1 FROM AuthorityProfileStaffTargets x WHERE x.WorldID=p.WorldID AND x.AuthorityProfileID=p.AuthorityProfileID AND x.TargetRoleID=?)))SQL",
                         {s.world, t.org, t.id, cap, targetRole, targetRole}) > 0;
    }
    std::int64_t shared(Session& s, const Team& t)
    {
        return t.venue
                   ? S::scalar(
                         s.db,
                         "SELECT count(DISTINCT ts.WorldOrganizationID) FROM TeamSeasons ts JOIN "
                         "CompetitionSeasons cs ON cs.CompetitionSeasonID=ts.CompetitionSeasonID JOIN "
                         "WorldOrganizations o ON o.WorldOrganizationID=ts.WorldOrganizationID JOIN "
                         "WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID WHERE "
                         "o.WorldID=? "
                         "AND c.WorldID=o.WorldID AND ts.WorldVenueID=? AND ts.WorldOrganizationID<>? AND "
                         "ts.Status='ACTIVE' AND cs.Status='ACTIVE' AND o.Active=1 AND c.Active=1",
                         {s.world, t.venue, t.org})
                   : 0;
    }
    void protectTeamInputs(Session& s,const Team& t)
    {
        if(!s.clockModel)return;
        require(S::scalar(s.db,"SELECT count(*) FROM CausalReadFences f JOIN Games g ON g.WorldID=f.WorldID AND CAST(g.GameID AS TEXT)=f.ResourceKey WHERE f.WorldID=? AND f.ResourceKind='GAME_INPUTS' AND f.ContextID<>? AND (g.AwayTeamSeasonID=? OR g.HomeTeamSeasonID=?) AND (f.AsOfUtc>? OR (f.AsOfUtc=? AND f.AsOfOrder>=?))",{s.world,s.context,t.id,t.id,s.now,s.now,s.contextOrder})==0,WorldEditError::CausalFence,"CAUSAL_FENCE: team changes conflict with already resolved game inputs");
    }
    // Validate UTF-8 code points rather than bytes/display cells. Reject malformed
    // sequences, controls, surrogates and overlong encodings; retain internal spaces.
    std::string text(std::string v, std::size_t maximum, bool required)
    {
        std::size_t count = 0, firstByte = std::string::npos, lastByte = 0, firstPoint = 0, lastPoint = 0;
        for (std::size_t i = 0; i < v.size();)
        {
            const auto byteStart = i;
            const unsigned char lead = v[i++];
            std::uint32_t cp = lead;
            int tails = 0;
            if (lead >= 0xC2 && lead <= 0xDF)
            {
                cp = lead & 31;
                tails = 1;
            }
            else if (lead >= 0xE0 && lead <= 0xEF)
            {
                cp = lead & 15;
                tails = 2;
            }
            else if (lead >= 0xF0 && lead <= 0xF4)
            {
                cp = lead & 7;
                tails = 3;
            }
            else
                require(lead < 128, WorldEditError::InvalidValue, "Malformed UTF-8");
            const auto bytes = tails;
            for (int j = 0; j < tails; ++j)
            {
                require(i < v.size() && (static_cast<unsigned char>(v[i]) & 0xC0) == 0x80,
                        WorldEditError::InvalidValue, "Malformed UTF-8");
                cp = (cp << 6) | (static_cast<unsigned char>(v[i++]) & 63);
            }
            require(!(bytes == 1 && cp < 128) && !(bytes == 2 && cp < 2048) && !(bytes == 3 && cp < 65536) &&
                        cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF),
                    WorldEditError::InvalidValue, "Invalid Unicode scalar");
            require(cp >= 32 && !(cp >= 127 && cp <= 159) && cp != 0x2028 && cp != 0x2029,
                    WorldEditError::InvalidValue, "Control characters are not allowed");
            const bool space = cp == 32 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
                               cp == 0x202F || cp == 0x205F || cp == 0x3000;
            if (!space)
            {
                if (firstByte == std::string::npos)
                {
                    firstByte = byteStart;
                    firstPoint = count;
                }
                lastByte = i;
                lastPoint = count + 1;
            }
            ++count;
        }
        count = firstByte == std::string::npos ? 0 : lastPoint - firstPoint;
        require(count <= maximum && (!required || count > 0), WorldEditError::InvalidValue,
                "Text is empty or exceeds the field limit");
        return firstByte == std::string::npos ? std::string{} : v.substr(firstByte, lastByte - firstByte);
    }
    std::optional<std::string> optionalText(const std::optional<std::string>& value, std::size_t max)
    {
        if (!value)
            return {};
        auto v = text(*value, max, false);
        return v.empty() ? std::nullopt : std::optional<std::string>(v);
    }
    std::string personKey()
    {
        std::random_device random;
        std::array<unsigned char, 16> b{};
        for (auto& x : b)
            x = static_cast<unsigned char>(random());
        b[6] = (b[6] & 15) | 64;
        b[8] = (b[8] & 63) | 128;
        const char* hex = "0123456789abcdef";
        std::string out = "user:";
        for (std::size_t i = 0; i < b.size(); ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                out += '-';
            out += hex[b[i] >> 4];
            out += hex[b[i] & 15];
        }
        return out;
    }
    void management(Session& s, const Team& t, bool enabled, std::int64_t rev, WorldEditCommit& result)
    {
        require(s.manage, WorldEditError::NotAuthorized, "This user cannot change team management");
        S::Query current(s.db,
                         "SELECT TeamManagementID,WorldUserID FROM TeamManagementIntents WHERE WorldID=? AND "
                         "WorldOrganizationID=? AND EndsUtc IS NULL",
                         {s.world, t.org});
        std::int64_t intent = 0;
        if (current.row())
        {
            intent = current.integer();
            require(current.integer(1) == s.user, WorldEditError::ControlConflict,
                    "Another user manages this team");
        }
        if (!enabled)
        {
            if (!intent)
                return;
            S::run(s.db,
                   "UPDATE CharacterRoleControls SET EndsUtc=? WHERE TeamManagementID=? AND EndsUtc IS NULL",
                   {s.now, intent});
            S::run(s.db, "UPDATE TeamManagementIntents SET EndsUtc=? WHERE TeamManagementID=?",
                   {s.now, intent});
            result.changed = true;
            return;
        }
        if (!intent)
        {
            S::run(
                s.db,
                "INSERT INTO "
                "TeamManagementIntents(WorldID,WorldOrganizationID,WorldUserID,CoveragePolicyKey,StartsUtc) "
                "VALUES(?,?,?,'ALL_SUPPORTED_TEAM_POSITIONS_V1',?)",
                {s.world, t.org, s.user, s.now});
            intent = S::inserted(s.db);
            result.changed = true;
        }
        S::Query positions(
            s.db,
            "SELECT p.PositionID,a.RoleAssignmentID,a.PersonID FROM RolePositions p JOIN RoleDefinitions r "
            "ON "
            "r.WorldID=p.WorldID AND r.RoleID=p.RoleID LEFT JOIN RoleAssignments a ON "
            "a.PositionID=p.PositionID "
            "AND a.EndsUtc IS NULL WHERE p.WorldID=? AND p.WorldOrganizationID=? AND p.Active=1 AND "
            "(p.TeamSeasonID IS NULL OR p.TeamSeasonID=?) AND r.RoleKey IN "
            "('OWNER','GENERAL_MANAGER','HEAD_COACH','OFFENSIVE_COORDINATOR','DEFENSIVE_COORDINATOR')",
            {s.world, t.org, t.id});
        while (positions.row())
        {
            auto pos = positions.integer(), assignment = positions.integer(1), person = positions.integer(2);
            S::run(s.db, "INSERT OR IGNORE INTO TeamManagementPositions VALUES(?,?,?,?)",
                   {s.world, intent, t.org, pos});
            if (sqlite3_changes(s.db))
                result.changed = true;
            if (!assignment)
                continue;
            S::Query character(
                s.db,
                "SELECT CharacterID,WorldUserID FROM Characters WHERE WorldID=? AND PersonID=? "
                "AND EndsUtc IS NULL",
                {s.world, person});
            std::int64_t id = 0;
            if (character.row())
            {
                id = character.integer();
                require(character.integer(1) == s.user, WorldEditError::ControlConflict,
                        "Incoming staff member is controlled by another user");
            }
            else
            {
                S::run(s.db, "INSERT INTO Characters(WorldID,WorldUserID,PersonID,StartsUtc) VALUES(?,?,?,?)",
                       {s.world, s.user, person, s.now});
                id = S::inserted(s.db);
                result.people.push_back(person);
            }
            if (!S::scalar(s.db,
                           "SELECT count(*) FROM CharacterRoleControls WHERE CharacterID=? AND "
                           "RoleAssignmentID=? AND TeamManagementID=? AND EndsUtc IS NULL",
                           {id, assignment, intent}))
            {
                S::run(s.db,
                       "INSERT INTO "
                       "CharacterRoleControls(WorldID,CharacterID,WorldUserID,PersonID,RoleAssignmentID,"
                       "PositionID,"
                       "OriginKind,TeamManagementID,StartsUtc) VALUES(?,?,?,?,?,?,'TEAM_MANAGEMENT',?,?)",
                       {s.world, id, s.user, person, assignment, pos, intent, s.now});
                result.changed = true;
            }
        }
        (void)rev;
    }
#include "GCE/Database/WorldVenueEvents.inl"

    template <class F>
    WorldEditResult<WorldEditCommit> write(const WorldActor& actor, std::int64_t expected, F&& fn,
                                           const std::string& commandKey = {},
                                           const std::string& fingerprint = {})
    {
        return attempt<WorldEditCommit>([&] {
            Session s(actor, true);
            S::Transaction transaction(s.db);
            s.refresh();
            require(worldEventContractsReady(s.db, s.world), WorldEditError::UnsupportedSchema,
                    "Unsupported dated-event contracts");
            require(S::utc(s.now) && worldEventPublicationReady(s.db, s.world), WorldEditError::IntegrityFailure,
                    "Invalid World clock or incomplete dated-event publication");
            if (auto replay = replayVenueCommand(s, actor, commandKey, fingerprint))
            {
                transaction.commit();
                return *replay;
            }
            require(s.revision == expected, WorldEditError::StaleRevision,
                    "The World changed. Reload this form before saving");
            require(s.revision < std::numeric_limits<std::int64_t>::max(), WorldEditError::IntegrityFailure,
                    "Revision exhausted");
            WorldEditCommit result;
            result.worldId = s.world;
            result.worldKey = WorldEditingAccess::worldKey(actor);
            result.revision = s.revision;
            result.currentTimeUtc = s.now;
            if(s.clockModel)
            {
                S::exec(s.db,"SAVEPOINT clock_edit");
                s.context=ClockStateSql::editContext(s.db,s.world,s.user,s.now,s.revision+1,s.god);
                s.contextOrder=S::scalar(s.db,"SELECT OrderInInstant FROM WorldExecutionContexts WHERE WorldID=? AND ContextID=?",{s.world,s.context});
            }
            fn(s, s.revision + 1, result);
            if(s.clockModel && !result.changed) S::exec(s.db,"ROLLBACK TO clock_edit");
            if(s.clockModel) S::exec(s.db,"RELEASE clock_edit");
            if (result.changed)
            {
                S::run(s.db, "UPDATE WorldDataRevisions SET Revision=? WHERE WorldID=? AND Revision=?",
                       {s.revision + 1, s.world, s.revision});
                require(sqlite3_changes(s.db) == 1, WorldEditError::StaleRevision, "World revision changed");
                result.revision = s.revision + 1;
            }
            require(worldEventPublicationReady(s.db, s.world), WorldEditError::IntegrityFailure,
                    "Incomplete dated-event publication");
            if (result.changed) result.committedRevision = result.revision;
            transaction.commit();
            return result;
        });
    }
} // namespace
WorldEditResult<WorldActor> openLocalWorldActor(const std::filesystem::path& path)
{
    return attempt<WorldActor>([&] {
        auto loaded = loadWorldDatabase(path);
        require(loaded.success,
                (loaded.schemaVersion != WorldDatabaseSchemaVersion ||
                 loaded.errorMessage.rfind("UNSUPPORTED_SCHEMA", 0) == 0)
                    ? WorldEditError::UnsupportedSchema
                    : WorldEditError::IntegrityFailure,
                loaded.errorMessage.c_str());
        Database d;
        require(d.open(path), WorldEditError::IoError, "Cannot open World");
        auto* db = WorldEditingAccess::handle(d);
        S::Query q(db,
                   "SELECT WorldUserID,UserKey FROM WorldUsers WHERE WorldID=? AND UserKey='LOCAL_PRIMARY' "
                   "AND Active=1",
                   {loaded.world.worldId});
        require(q.row(), WorldEditError::NotAuthorized, "Local user is unavailable");
        WorldActor actor;
        actor.database_ = std::filesystem::absolute(path).lexically_normal();
        actor.world_ = loaded.world.worldId;
        actor.worldKey_ = loaded.world.worldKey;
        actor.user_ = q.integer();
        actor.userKey_ = q.text(1);
        return actor;
    });
}
static WorldEditResult<WorldTeamData> readTeamData(const WorldActor& actor, std::int64_t id,
                                                   std::optional<bool> preview)
{
    return attempt<WorldTeamData>([&] {
        Session s(actor, false);
        auto t = team(s, id);
        WorldTeamData r;
        r.worldId = s.world;
        r.organizationId = t.org;
        r.teamSeasonId = t.id;
        r.venueId = t.venue;
        r.revision = s.revision;
        r.currentTimeUtc = s.now;
        r.city = t.city;
        r.nickname = t.nickname;
        r.displayName = t.name;
        r.venueName = t.venueName;
        r.surface = t.surface;
        r.godMode = s.god;
        r.canManage = s.manage;
        r.managed = S::scalar(s.db,
                              "SELECT count(*) FROM TeamManagementIntents WHERE WorldID=? AND "
                              "WorldOrganizationID=? AND WorldUserID=? AND EndsUtc IS NULL",
                              {s.world, t.org, s.user}) > 0;
        r.otherVenueUsers = shared(s, t);
        r.leagueWeek = weekProgress(s, t.season);
        r.calendarReady = true; // Dated requests do not require week membership or a successor.
        if (t.venue)
            r.pendingVenueChanges = venueEvents(s, t.venue, 0, 500, true);
        if (preview)
        {
            require(s.manage, WorldEditError::NotAuthorized, "This user cannot change team management");
            S::Query owner(s.db,
                           "SELECT WorldUserID FROM TeamManagementIntents WHERE WorldID=? AND "
                           "WorldOrganizationID=? AND EndsUtc IS NULL",
                           {s.world, t.org});
            if (owner.row())
                require(owner.integer() == s.user, WorldEditError::ControlConflict,
                        "Another user manages this team");
            if (*preview)
                require(S::scalar(s.db,
                                  "SELECT count(*) FROM RolePositions p JOIN RoleDefinitions r ON "
                                  "r.WorldID=p.WorldID AND r.RoleID=p.RoleID JOIN RoleAssignments a ON "
                                  "a.PositionID=p.PositionID AND a.EndsUtc IS NULL JOIN Characters ch ON "
                                  "ch.WorldID=p.WorldID AND ch.PersonID=a.PersonID AND ch.EndsUtc IS NULL "
                                  "WHERE p.WorldID=? AND p.WorldOrganizationID=? AND p.Active=1 AND "
                                  "(p.TeamSeasonID IS NULL OR p.TeamSeasonID=?) AND r.RoleKey IN "
                                  "('OWNER','GENERAL_MANAGER','HEAD_COACH','OFFENSIVE_COORDINATOR','"
                                  "DEFENSIVE_COORDINATOR') AND ch.WorldUserID<>?",
                                  {s.world, t.org, t.id, s.user}) == 0,
                        WorldEditError::ControlConflict, "Staff are controlled by another user");
            r.managed = *preview;
        }
        r.canEditIdentity = capability(s, t, "TEAM_IDENTITY_EDIT", 0, preview);
        r.canEditVenue = t.venue && capability(s, t, "VENUE_EDIT", 0, preview) &&
                         (s.god || !r.otherVenueUsers) && r.calendarReady;
        S::Query q(
            s.db,
            "SELECT "
            "p.PositionID,p.RoleID,r.RoleKey,r.DisplayName,a.RoleAssignmentID,a.PersonID,h.DisplayName FROM "
            "RolePositions p JOIN RoleDefinitions r ON r.WorldID=p.WorldID AND r.RoleID=p.RoleID LEFT JOIN "
            "RoleAssignments a ON a.PositionID=p.PositionID AND a.EndsUtc IS NULL LEFT JOIN People h ON "
            "h.PersonID=a.PersonID AND h.WorldID=p.WorldID WHERE p.WorldID=? AND p.WorldOrganizationID=? AND "
            "p.Active=1 AND (p.TeamSeasonID IS NULL OR p.TeamSeasonID=?) ORDER BY p.PositionID",
            {s.world, t.org, t.id});
        while (q.row())
        {
            WorldStaffPosition p;
            p.positionId = q.integer();
            p.roleId = q.integer(1);
            p.roleKey = q.text(2);
            p.displayName = q.text(3);
            p.assignmentId = q.integer(4);
            p.personId = q.integer(5);
            p.personName = q.text(6);
            p.editable = capability(s, t, "STAFF_ASSIGN", p.roleId, preview);
            p.userControlled =
                p.assignmentId && S::scalar(s.db,
                                            "SELECT count(*) FROM CharacterRoleControls WHERE WorldID=? AND "
                                            "WorldUserID=? AND RoleAssignmentID=? AND EndsUtc IS NULL",
                                            {s.world, s.user, p.assignmentId}) > 0;
            if (!p.assignmentId)
                r.warnings.push_back(p.displayName + ": unassigned; source identity may be incomplete");
            r.staff.push_back(p);
        }
        S::Query provenance(
            s.db,
            "SELECT snap.SnapshotPath FROM TeamSeasons ts JOIN CompetitionSeasons cs ON "
            "cs.CompetitionSeasonID=ts.CompetitionSeasonID JOIN WorldCoreSnapshots snap ON "
            "snap.WorldCoreSnapshotID=cs.WorldCoreSnapshotID WHERE ts.TeamSeasonID=? AND snap.WorldID=?",
            {t.id, s.world});
        if (provenance.row())
        {
            auto snapshot = std::filesystem::u8path(provenance.text());
            if (snapshot.is_relative())
                snapshot = WorldEditingAccess::path(actor).parent_path() / snapshot;
            Database core;
            require(core.open(snapshot), WorldEditError::IoError, "Preserved Core metadata is unavailable");
            S::Query warnings(WorldEditingAccess::handle(core),
                              "SELECT MetadataValue FROM CoreMetadata WHERE MetadataKey LIKE "
                              "'BuildWarning.%.Message' ORDER BY MetadataKey");
            while (warnings.row())
                r.warnings.push_back("Core warning: " + warnings.text());
        }
        if (!r.calendarReady)
            r.warnings.push_back(
                "DATED_EVENTS_UNAVAILABLE: venue scheduling is unavailable");
        if (r.otherVenueUsers && !s.god)
            r.warnings.push_back("Shared venue: name and surface are read-only");
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<WorldTeamData> loadTeamData(const WorldActor& actor, std::int64_t id)
{
    return readTeamData(actor, id, {});
}
WorldEditResult<WorldTeamData> previewTeamManagement(const WorldActor& actor, std::int64_t id, bool enabled)
{
    return readTeamData(actor, id, enabled);
}
WorldEditResult<WorldEditState> loadWorldEditState(const WorldActor& actor)
{
    return attempt<WorldEditState>([&] {
        Session s(actor, false);
        WorldEditState state;
        state.worldId = s.world;
        state.revision = s.revision;
        state.godMode = s.god;
        state.canEnableGodMode = s.enableGod;
        return state;
    });
}
WorldEditResult<std::vector<WorldPersonChoice>> listAvailablePeople(const WorldActor& actor, std::int64_t org,
                                                                    std::size_t offset, std::size_t limit)
{
    return attempt<std::vector<WorldPersonChoice>>([&] {
        Session s(actor, false);
        require(limit > 0 && limit <= 500 && offset <= 1000000, WorldEditError::InvalidValue, "Invalid page");
        require(S::scalar(s.db,
                          "SELECT count(*) FROM WorldOrganizations WHERE WorldID=? AND WorldOrganizationID=? "
                          "AND Active=1",
                          {s.world, org}) == 1,
                WorldEditError::TeamNotActive, "Organization unavailable");
        S::Query q(
            s.db,
            "SELECT p.PersonID,p.DisplayName FROM People p WHERE p.WorldID=? AND p.Active=1 AND NOT "
            "EXISTS(SELECT 1 FROM RoleAssignments a WHERE a.WorldID=p.WorldID AND a.PersonID=p.PersonID AND "
            "a.EndsUtc IS NULL) AND NOT EXISTS(SELECT 1 FROM WorldPlayers wp WHERE wp.PersonID=p.PersonID) "
            "ORDER BY p.DisplayName,p.PersonID LIMIT ? OFFSET ?",
            {s.world, static_cast<std::int64_t>(limit), static_cast<std::int64_t>(offset)});
        std::vector<WorldPersonChoice> out;
        while (q.row())
            out.push_back({q.integer(), q.text(1)});
        return out;
    });
}
WorldEditResult<WorldEditCommit> saveTeamData(const WorldActor& actor, const WorldTeamDataDraft& draft)
{
    return attempt<WorldEditCommit>([&] {
        const auto fingerprint = saveFingerprint(draft);
        auto saved = write(
            actor, draft.expectedRevision,
            [&](Session& s, std::int64_t rev, WorldEditCommit& result) {
                auto t = team(s, draft.teamSeasonId);
                require(draft.worldId == s.world && draft.organizationId == t.org,
                        WorldEditError::InvalidValue, "Draft scope does not match the selected team");
                if(draft.managed && s.clockModel)
                {
                    const bool current=S::scalar(s.db,"SELECT count(*) FROM TeamManagementIntents WHERE WorldID=? AND WorldOrganizationID=? AND EndsUtc IS NULL",{s.world,t.org})>0;
                    if(current!=*draft.managed)protectTeamInputs(s,t);
                }
                if (draft.managed && *draft.managed)
                    management(s, t, true, rev, result);
                auto city = draft.city ? text(*draft.city, 128, true) : t.city,
                     nickname = draft.nickname ? text(*draft.nickname, 128, true) : t.nickname;
                const bool identityChange = city != t.city || nickname != t.nickname;
                if(identityChange)protectTeamInputs(s,t);
                saveVenue(s, t, draft, fingerprint, rev, result);
                if (identityChange)
                    require(capability(s, t, "TEAM_IDENTITY_EDIT"), WorldEditError::NotAuthorized,
                            "Team identity requires controlled owner authority or God mode");
                struct Pending
                {
                    WorldStaffAction action;
                    std::int64_t role = 0, oldPerson = 0, newPerson = 0;
                };
                std::vector<Pending> pending;
                std::set<std::int64_t> seen, chosen;
                // Validate every choice and permission against the same pre-save state.
                for (const auto& a : draft.staff)
                {
                    require(seen.insert(a.positionId).second, WorldEditError::InvalidValue,
                            "Duplicate staff position in draft");
                    S::Query q(
                        s.db,
                        "SELECT p.RoleID,a.RoleAssignmentID,a.PersonID,r.RoleKey FROM RolePositions p JOIN "
                        "RoleDefinitions r ON r.WorldID=p.WorldID AND r.RoleID=p.RoleID LEFT JOIN "
                        "RoleAssignments a ON a.PositionID=p.PositionID AND a.EndsUtc IS NULL WHERE "
                        "p.WorldID=? AND p.WorldOrganizationID=? AND p.PositionID=? AND p.Active=1 AND "
                        "(p.TeamSeasonID IS NULL OR p.TeamSeasonID=?)",
                        {s.world, t.org, a.positionId, t.id});
                    require(q.row(), WorldEditError::InvalidValue, "Staff position is outside this team");
                    require(q.integer(1) == a.expectedAssignmentId, WorldEditError::StaleRoleHolder,
                            "Staff holder changed; reload the form");
                    if (a.kind == WorldStaffActionKind::Keep)
                    {
                        require(!a.personId && !a.newPerson, WorldEditError::InvalidValue,
                                "KEEP cannot replace a person");
                        continue;
                    }
                    const auto role = q.integer();
                    const auto key = q.text(3);
                    require(key == "OWNER" || key == "GENERAL_MANAGER" || key == "HEAD_COACH" ||
                                key == "OFFENSIVE_COORDINATOR" || key == "DEFENSIVE_COORDINATOR",
                            WorldEditError::UnsupportedFeature,
                            "This role is not supported by the initial team editor");
                    require(capability(s, t, "STAFF_ASSIGN", role), WorldEditError::NotAuthorized,
                            "This controlled role cannot edit the selected staff position");
                    if (a.kind == WorldStaffActionKind::Remove)
                    {
                        require(!a.personId && !a.newPerson, WorldEditError::InvalidValue,
                                "REMOVE cannot select a replacement");
                        if (a.expectedAssignmentId)
                            pending.push_back({a, role, q.integer(2), 0});
                        continue;
                    }
                    require(a.kind == WorldStaffActionKind::Replace &&
                                (a.personId.has_value() != a.newPerson.has_value()),
                            WorldEditError::InvalidValue,
                            "Select one existing person or create one new person");
                    if (a.personId)
                    {
                        require(chosen.insert(*a.personId).second, WorldEditError::PersonUnavailable,
                                "A replacement cannot fill multiple jobs in this workflow");
                        require(
                            S::scalar(
                                s.db,
                                "SELECT count(*) FROM People p WHERE p.WorldID=? AND p.PersonID=? AND "
                                "p.Active=1 AND NOT EXISTS(SELECT 1 FROM RoleAssignments a WHERE "
                                "a.WorldID=p.WorldID AND a.PersonID=p.PersonID AND a.EndsUtc IS NULL) "
                                "AND NOT EXISTS(SELECT 1 FROM WorldPlayers wp WHERE wp.PersonID=p.PersonID)",
                                {s.world, *a.personId}) == 1,
                            WorldEditError::PersonUnavailable,
                            "Replacement must be an available staff person without a player identity or "
                            "active job");
                    }
                    pending.push_back({a, role, q.integer(2), a.personId.value_or(0)});
                }
                if(!pending.empty())protectTeamInputs(s,t);
                for (auto& p : pending)
                {
                    if (p.action.newPerson)
                    {
                        const auto& n = *p.action.newPerson;
                        const auto name = text(n.displayName, 256, true);
                        const auto first = optionalText(n.firstName, 128),
                                   last = optionalText(n.lastName, 128);
                        S::run(s.db,
                               "INSERT INTO People(WorldID,PersonKey,DisplayName,FirstName,LastName) "
                               "VALUES(?,?,?,?,?)",
                               {s.world, personKey(), name, S::nullable(first), S::nullable(last)});
                        p.newPerson = S::inserted(s.db);
                        result.people.push_back(p.newPerson);
                    }
                    if (p.action.expectedAssignmentId)
                    {
                        S::run(s.db,
                               "UPDATE CharacterRoleControls SET EndsUtc=? WHERE WorldID=? AND "
                               "RoleAssignmentID=? "
                               "AND EndsUtc IS NULL",
                               {s.now, s.world, p.action.expectedAssignmentId});
                        S::run(s.db,
                               "UPDATE RoleAssignments SET "
                               "EndsUtc=?,EndsRevision=?,EndReasonCode='TEAM_DATA_EDIT' "
                               "WHERE WorldID=? AND RoleAssignmentID=? AND EndsUtc IS NULL",
                               {s.now, rev, s.world, p.action.expectedAssignmentId});
                        result.people.push_back(p.oldPerson);
                    }
                    if (p.newPerson)
                    {
                        S::run(s.db,
                               "INSERT INTO "
                               "RoleAssignments(WorldID,PositionID,PersonID,StartsUtc,StartsRevision) "
                               "VALUES(?,?,?,?,?)",
                               {s.world, p.action.positionId, p.newPerson, s.now, rev});
                        result.people.push_back(p.newPerson);
                    }
                    result.changed = true;
                }
                if (identityChange)
                {
                    const auto name = city + " " + nickname;
                    S::run(
                        s.db,
                        "UPDATE TeamSeasons SET MarketName=?,Nickname=?,DisplayName=? WHERE TeamSeasonID=?",
                        {city, nickname, name, t.id});
                    S::run(s.db,
                           "UPDATE WorldOrganizations SET CanonicalName=? WHERE WorldID=? AND "
                           "WorldOrganizationID=?",
                           {name, s.world, t.org});
                    S::run(
                        s.db,
                        "INSERT INTO OrganizationIdentityOverrides VALUES(?,?,?,?,?,?,?) ON "
                        "CONFLICT(WorldID,WorldOrganizationID) DO UPDATE SET "
                        "MarketName=excluded.MarketName,Nickname=excluded.Nickname,DisplayName=excluded."
                        "DisplayName,ChangedUtc=excluded.ChangedUtc,ChangedRevision=excluded.ChangedRevision",
                        {s.world, t.org, city, nickname, name, s.now, rev});
                    result.changed = true;
                }
                if (draft.managed && !*draft.managed)
                    management(s, t, false, rev, result);
                else if (!pending.empty())
                {
                    // Staff turnover preserves the intent owner's control, including when
                    // a different Commissioner makes the authorized edit.
                    S::Query owner(s.db,
                                   "SELECT WorldUserID FROM TeamManagementIntents WHERE WorldID=? AND "
                                   "WorldOrganizationID=? AND EndsUtc IS NULL",
                                   {s.world, t.org});
                    if (owner.row())
                    {
                        const auto actorUser = s.user;
                        const bool actorManage = s.manage;
                        s.user = owner.integer();
                        s.manage = true;
                        management(s, t, true, rev, result);
                        s.user = actorUser;
                        s.manage = actorManage;
                    }
                }
                if (result.changed)
                {
                    result.organizations.push_back(t.org);
                    result.teamSeasons.push_back(t.id);
                }
            },
            draft.venueCommandKey, fingerprint);
        if (!saved.success)
            throw Failure(saved.error, saved.message);
        return saved.value;
    });
}
WorldEditResult<WorldEditCommit> setTeamManagement(const WorldActor& actor, std::int64_t org, bool enabled,
                                                   std::int64_t expected)
{
    // Selection is revalidated by saveTeamData inside its write transaction.
    return attempt<WorldEditCommit>([&] {
        std::int64_t world = 0, id = 0;
        {
            Session s(actor, false);
            world = s.world;
            S::Query q(s.db,
                       "SELECT t.TeamSeasonID FROM TeamSeasons t JOIN CompetitionSeasons c ON "
                       "c.CompetitionSeasonID=t.CompetitionSeasonID JOIN WorldOrganizations o ON "
                       "o.WorldOrganizationID=t.WorldOrganizationID WHERE o.WorldID=? AND "
                       "t.WorldOrganizationID=? AND t.Status='ACTIVE' AND c.Status='ACTIVE'",
                       {s.world, org});
            require(q.row(), WorldEditError::TeamNotActive, "Active team unavailable");
            id = q.integer();
            require(!q.row(), WorldEditError::UnsupportedFeature,
                    "Use a specific TeamSeason draft when multiple seasons are active");
        }
        WorldTeamDataDraft d;
        d.worldId = world;
        d.organizationId = org;
        d.teamSeasonId = id;
        d.expectedRevision = expected;
        d.managed = enabled;
        auto r = saveTeamData(actor, d);
        if (!r.success)
            throw Failure(r.error, r.message);
        return r.value;
    });
}
WorldEditResult<WorldEditCommit> setWorldEditMode(const WorldActor& actor, bool enabled,
                                                  std::int64_t expected)
{
    return write(actor, expected, [&](Session& s, std::int64_t rev, WorldEditCommit& r) {
        require(s.enableGod, WorldEditError::NotAuthorized, "This user cannot enable God mode");
        if (s.god == enabled)
            return;
        S::run(s.db,
               "UPDATE WorldUserEditModes SET GodModeEnabled=?,ChangedUtc=?,ChangedRevision=? WHERE "
               "WorldID=? AND WorldUserID=?",
               {static_cast<std::int64_t>(enabled), s.now, rev, s.world, s.user});
        r.changed = true;
    });
}
WorldEditResult<WorldLeagueWeekProgress> loadLeagueWeekProgress(const WorldActor& actor, std::int64_t season)
{
    return attempt<WorldLeagueWeekProgress>([&] {
        Session s(actor, false);
        auto r = weekProgress(s, season);
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<std::vector<WorldVenueEvent>> listVenueEvents(const WorldActor& actor, std::int64_t venue,
                                                              std::size_t offset, std::size_t limit)
{
    return attempt<std::vector<WorldVenueEvent>>([&] {
        Session s(actor, false);
        auto r = venueEvents(s, venue, offset, limit);
        S::exec(s.db, "COMMIT");
        return r;
    });
}
WorldEditResult<std::vector<WorldVenueHistoryEntry>> listVenueHistory(const WorldActor& actor,
                                                                      std::int64_t venue, std::size_t offset,
                                                                      std::size_t limit)
{
    return attempt<std::vector<WorldVenueHistoryEntry>>([&] {
        auto events = listVenueEvents(actor, venue, offset, limit);
        if (!events.success)
            throw Failure(events.error, events.message);
        std::vector<WorldVenueHistoryEntry> rows;
        for (const auto& e : events.value)
        {
            WorldVenueHistoryEntry r;
            r.event = e;
            r.periodId = e.planId;
            r.periodKey = e.dueUtc + " " + e.field + " " + e.state;
            r.startsUtc = e.announcedUtc;
            r.endsUtc = e.resolvedUtc;
            r.closed = e.state != "PENDING";
            r.hasNetChange = e.state == "APPLIED" && e.desiredValue != e.appliedBeforeValue;
            if (e.field == "NAME")
            {
                r.openingName = e.appliedBeforeValue.value_or("");
                r.finalName = e.desiredValue.value_or("");
            }
            else
            {
                r.openingSurface = e.appliedBeforeValue;
                r.finalSurface = e.desiredValue;
            }
            rows.push_back(std::move(r));
        }
        return rows;
    });
}
WorldEditResult<WorldEditCommit> cancelVenueChanges(const WorldActor& actor, std::int64_t teamId,
                                                    const std::vector<std::int64_t>& eventIds,
                                                    const std::string& key, std::int64_t expected)
{
    return attempt<WorldEditCommit>([&] {
        auto ids = eventIds;
        std::sort(ids.begin(), ids.end());
        require(!ids.empty() && ids.size() <= 2 && std::adjacent_find(ids.begin(), ids.end()) == ids.end(),
                WorldEditError::InvalidValue, "Select one or two distinct pending venue requests");
        std::string fingerprint = "venue-cancel-v2:";
        fingerprintPart(fingerprint, std::to_string(teamId));
        for (auto id : ids)
            fingerprintPart(fingerprint, std::to_string(id));
        auto r = write(
            actor, expected,
            [&](Session &s, std::int64_t rev, WorldEditCommit &result) {
                auto t = team(s, teamId);
                require(t.venue && capability(s, t, "VENUE_EDIT"), WorldEditError::NotAuthorized,
                        "Current venue authority is required to cancel requests");
                require(s.god || shared(s, t) == 0, WorldEditError::SharedVenueReadOnly,
                        "Shared venue requests are read-only outside God mode");
                const auto pending = venueEvents(s, t.venue, 0, 500, true);
                for (auto id : ids)
                {
                    const auto p = std::find_if(pending.begin(), pending.end(),
                                                [&](const auto &e) { return e.eventId == id; });
                    require(p != pending.end(), WorldEditError::StalePendingEvent,
                            "Pending request changed or is outside this team's venue");
                    require(s.god || s.now < p->dueUtc, WorldEditError::DueWorkPending,
                            "Due venue work must be processed before ordinary cancellation");
                }
                const auto command = s.god ? 0 : venueCommand(s, t, key, fingerprint, rev);
                for (auto id : ids)
                {
                    const auto p = std::find_if(pending.begin(), pending.end(),
                                                [&](const auto &e) { return e.eventId == id; });
                    resolveVenueRequest(s, *p, "CANCELED", s.god ? "GOD_OVERRIDE" : "USER_CANCEL", command,
                                        rev);
                    if (!s.god)
                        venueFact(s, p->eventId, p->scheduledEventId, "VENUE_CANCELED", rev);
                }
                result.changed = true;
                result.venues.push_back(t.venue);
                result.organizations.push_back(t.org);
                result.teamSeasons.push_back(t.id);
            },
            key, fingerprint);
        if (!r.success)
            throw Failure(r.error, r.message);
        return r.value;
    });
}
WorldEditResult<WorldEditCommit> advanceWorldClock(const WorldActor &actor, const std::string &utc,
                                                   std::int64_t expected)
{
    return attempt<WorldEditCommit>([&]() -> WorldEditCommit {
        Session s(actor, false);
        require(S::utc(utc) && utc >= s.now, WorldEditError::InvalidValue,
                "World time must be valid UTC and cannot move backward");
        throw Failure(
            WorldEditError::UnsupportedFeature,
            "Direct clock advancement is disabled; use beginWorldAdvance and continueWorldAdvance");
    });
}
WorldEditResult<WorldEditCommit> captureGameVenueEvidence(const WorldActor &actor, std::int64_t game,
                                                          std::int64_t expected)
{
    return write(actor, expected, [&](Session &s, std::int64_t rev, WorldEditCommit &r) {
        require(!s.clockModel,WorldEditError::UnsupportedFeature,"CLOCK_SCHEDULER_STAGE2_REQUIRED: venue capture must bind admitted game inputs");
        require(s.manage, WorldEditError::NotAuthorized, "This session cannot start games");
        S::Query q(s.db,
                   "SELECT g.WorldVenueID,v.VenueName,v.TurfType,g.GameStatus,g.StartedUtc FROM Games g LEFT "
                   "JOIN WorldVenues v ON v.WorldID=g.WorldID AND v.WorldVenueID=g.WorldVenueID WHERE "
                   "g.WorldID=? AND g.GameID=?",
                   {s.world, game});
        require(q.row(), WorldEditError::InvalidValue, "Game is outside this World");
        if (S::scalar(s.db, "SELECT count(*) FROM GameVenueEvidence WHERE WorldID=? AND GameID=?",
                      {s.world, game}))
            return;
        require(q.text(3) == "SCHEDULED" || q.text(3) == "IN_PROGRESS", WorldEditError::UnsupportedFeature,
                "Capture venue evidence before game play, never after finalization");
        require(q.null(4) || q.text(4) == s.now, WorldEditError::InvalidValue,
                "Venue evidence must be captured at game start");
        require(S::scalar(s.db, "SELECT count(*) FROM GameArtifacts WHERE GameID=?", {game}) == 0,
                WorldEditError::InvalidValue, "Capture venue evidence before game artifacts exist");
        const bool unknown = q.null(0);
        require(unknown || !q.null(1), WorldEditError::IntegrityFailure, "Game venue is unresolved");
        S::run(s.db, "INSERT INTO GameVenueEvidence VALUES(?,?,?,?,?,?,?,?)",
               {game, s.world, unknown ? S::Value(nullptr) : S::Value(q.integer()),
                std::string(unknown ? "UNKNOWN_VENUE" : "CAPTURED"),
                unknown ? S::Value(nullptr) : S::Value(q.text(1)), S::nullable(q.optionalText(2)), s.now,
                rev});
        r.changed = true;
    });
}

#include "GCE/Database/WorldWeekTransaction.inl"

WorldGameWindow previewInstalledWindow(Session&,std::int64_t);
WorldEditResult<WorldAdvanceOutcome> beginInstalledGameWindow(const WorldActor&,std::int64_t,const std::string&,std::int64_t,const std::string&);
#include "GCE/Database/WorldScheduler.inl"
#include "GCE/Database/WorldClockServices.inl"
#include "GCE/Database/WorldTestGame.inl"
