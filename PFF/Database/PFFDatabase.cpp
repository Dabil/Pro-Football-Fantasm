#include "Database/PFFDatabase.h"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
    struct TagLine
    {
        std::string tag;
        std::string value;
        std::size_t line = 0;
    };

    class BuildException final : public std::runtime_error
    {
    public:
        BuildException(std::string message,
            std::filesystem::path sourceFile = {},
            std::size_t sourceLine = 0)
            : std::runtime_error(std::move(message)),
            sourceFile_(std::move(sourceFile)),
            sourceLine_(sourceLine)
        {
        }

        const std::filesystem::path& sourceFile() const noexcept
        {
            return sourceFile_;
        }

        std::size_t sourceLine() const noexcept
        {
            return sourceLine_;
        }

    private:
        std::filesystem::path sourceFile_;
        std::size_t sourceLine_ = 0;
    };

    std::string pathUtf8(const std::filesystem::path& path)
    {
        return path.u8string();
    }

    std::string trim(std::string value)
    {
        auto notSpace = [](unsigned char c)
            {
                return !std::isspace(c);
            };

        value.erase(
            value.begin(),
            std::find_if(value.begin(), value.end(), notSpace));

        value.erase(
            std::find_if(value.rbegin(), value.rend(), notSpace).base(),
            value.end());

        return value;
    }

    int parseInt(const std::string& value,
        const std::filesystem::path& sourceFile,
        std::size_t sourceLine,
        const char* fieldName)
    {
        int result = 0;
        const char* begin = value.data();
        const char* end = value.data() + value.size();

        const auto parsed = std::from_chars(begin, end, result);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
        {
            throw BuildException(
                "Invalid integer for " + std::string(fieldName) + ": '" + value + "'.",
                sourceFile,
                sourceLine);
        }

        return result;
    }

    std::vector<TagLine> readTaggedFile(const std::filesystem::path& file)
    {
        std::ifstream input(file);
        if (!input)
        {
            throw BuildException("Unable to open source file.", file);
        }

        std::vector<TagLine> tags;
        std::string line;
        std::size_t lineNumber = 0;

        while (std::getline(input, line))
        {
            ++lineNumber;

            if (lineNumber == 1 &&
                line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }

            std::string stripped = trim(line);

            if (stripped.empty() || stripped.rfind("//", 0) == 0)
            {
                continue;
            }

            if (stripped.front() != '[')
            {
                throw BuildException(
                    "Expected a tagged source line beginning with '['.",
                    file,
                    lineNumber);
            }

            const std::size_t close = stripped.find(']');
            if (close == std::string::npos)
            {
                throw BuildException(
                    "Malformed tag: missing ']'.",
                    file,
                    lineNumber);
            }

            TagLine tag;
            tag.tag = stripped.substr(1, close - 1);
            tag.value = trim(stripped.substr(close + 1));
            tag.line = lineNumber;

            tags.push_back(std::move(tag));
        }

        return tags;
    }

    class Statement
    {
    public:
        Statement(sqlite3* db, const char* sql)
        {
            const int rc = sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr);
            if (rc != SQLITE_OK)
            {
                throw BuildException(
                    "SQLite prepare failed: " + std::string(sqlite3_errmsg(db)));
            }
        }

        ~Statement()
        {
            if (statement_ != nullptr)
            {
                sqlite3_finalize(statement_);
            }
        }

        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        sqlite3_stmt* get() noexcept
        {
            return statement_;
        }

        void reset()
        {
            sqlite3_reset(statement_);
            sqlite3_clear_bindings(statement_);
        }

        void stepDone(sqlite3* db)
        {
            const int rc = sqlite3_step(statement_);
            if (rc != SQLITE_DONE)
            {
                throw BuildException(
                    "SQLite statement failed: " + std::string(sqlite3_errmsg(db)));
            }
        }

        bool stepRow(sqlite3* db)
        {
            const int rc = sqlite3_step(statement_);

            if (rc == SQLITE_ROW)
            {
                return true;
            }

            if (rc == SQLITE_DONE)
            {
                return false;
            }

            throw BuildException(
                "SQLite query failed: " + std::string(sqlite3_errmsg(db)));
        }

    private:
        sqlite3_stmt* statement_ = nullptr;
    };

    void bindText(sqlite3_stmt* statement, int index, const std::string& value)
    {
        if (sqlite3_bind_text(
            statement,
            index,
            value.c_str(),
            static_cast<int>(value.size()),
            SQLITE_TRANSIENT) != SQLITE_OK)
        {
            throw BuildException("SQLite text bind failed.");
        }
    }

    void bindInt(sqlite3_stmt* statement, int index, int value)
    {
        if (sqlite3_bind_int(statement, index, value) != SQLITE_OK)
        {
            throw BuildException("SQLite integer bind failed.");
        }
    }

    void bindInt64(sqlite3_stmt* statement, int index, sqlite3_int64 value)
    {
        if (sqlite3_bind_int64(statement, index, value) != SQLITE_OK)
        {
            throw BuildException("SQLite 64-bit integer bind failed.");
        }
    }

    void bindNull(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_bind_null(statement, index) != SQLITE_OK)
        {
            throw BuildException("SQLite null bind failed.");
        }
    }

    void exec(sqlite3* db, const char* sql)
    {
        char* error = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);

        if (rc != SQLITE_OK)
        {
            std::string message =
                error != nullptr ? error : sqlite3_errmsg(db);

            sqlite3_free(error);
            throw BuildException("SQLite exec failed: " + message);
        }
    }

    sqlite3_int64 lastInsertId(sqlite3* db)
    {
        return sqlite3_last_insert_rowid(db);
    }

    struct BuildContext
    {
        sqlite3* db = nullptr;
        CoreDatabaseBuildResult* result = nullptr;
        std::filesystem::path sourceRoot;

        sqlite3_int64 rulesetId = 0;
        sqlite3_int64 competitionId = 0;
        sqlite3_int64 competitionSeasonId = 0;

        std::unordered_map<std::string, sqlite3_int64> venueByKey;
        std::unordered_map<std::string, sqlite3_int64> teamByKey;
        std::unordered_map<std::string, sqlite3_int64> teamSeasonByKey;
        std::unordered_map<std::string, sqlite3_int64> teamSeasonBySourceName;
        std::unordered_map<std::string, sqlite3_int64> groupByKey;

        void inserted(std::uint64_t count = 1)
        {
            result->recordsInserted += count;
        }

        void diagnostic(DatabaseDiagnosticSeverity severity,
            std::string message,
            const std::filesystem::path& file = {},
            std::size_t line = 0)
        {
            DatabaseDiagnostic diagnostic;
            diagnostic.severity = severity;
            diagnostic.message = std::move(message);
            diagnostic.sourceFile = file;
            diagnostic.sourceLine = line;
            result->diagnostics.push_back(std::move(diagnostic));
        }
    };

    std::string sourceTeamNameKey(const std::string& city,
        const std::string& nickname)
    {
        return city + "\n" + nickname;
    }

    std::filesystem::path requireFile(
        const std::filesystem::path& directory,
        const char* fileName)
    {
        const std::filesystem::path file = directory / fileName;

        if (!std::filesystem::is_regular_file(file))
        {
            throw BuildException(
                "Required source file is missing: " + pathUtf8(file));
        }

        return file;
    }

    void createSchema(sqlite3* db)
    {
        exec(db, R"SQL(
CREATE TABLE CoreMetadata
(
    MetadataKey   TEXT PRIMARY KEY,
    MetadataValue TEXT NOT NULL
);

CREATE TABLE SourceFiles
(
    SourceFileID    INTEGER PRIMARY KEY,
    RelativePath    TEXT NOT NULL UNIQUE,
    DataFamily      TEXT NOT NULL,
    Status          TEXT NOT NULL,
    RecordsImported INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE Rulesets
(
    RulesetID  INTEGER PRIMARY KEY,
    RulesetKey TEXT NOT NULL UNIQUE
);

CREATE TABLE Venues
(
    VenueID       INTEGER PRIMARY KEY,
    VenueKey      TEXT NOT NULL UNIQUE,
    Name          TEXT NOT NULL,
    City          TEXT,
    State         TEXT,
    LatitudeText  TEXT,
    LongitudeText TEXT,
    TimeZone      TEXT,
    Surface       TEXT,
    StadiumType   TEXT,
    CapacityText  TEXT
);

CREATE TABLE VenueAliases
(
    VenueAliasID       INTEGER PRIMARY KEY,
    SourceStadiumLabel TEXT NOT NULL UNIQUE,
    VenueID            INTEGER NOT NULL,
    VenueName          TEXT,
    Provenance         TEXT,
    FOREIGN KEY (VenueID) REFERENCES Venues(VenueID)
);

CREATE TABLE Competitions
(
    CompetitionID  INTEGER PRIMARY KEY,
    CompetitionKey TEXT NOT NULL UNIQUE,
    DisplayName    TEXT NOT NULL
);

CREATE TABLE CompetitionSeasons
(
    CompetitionSeasonID INTEGER PRIMARY KEY,
    CompetitionID       INTEGER NOT NULL,
    SeasonYear          INTEGER NOT NULL,
    UNIQUE (CompetitionID, SeasonYear),
    FOREIGN KEY (CompetitionID) REFERENCES Competitions(CompetitionID)
);

CREATE TABLE CompetitionGroups
(
    CompetitionGroupID  INTEGER PRIMARY KEY,
    CompetitionSeasonID INTEGER NOT NULL,
    ParentGroupID       INTEGER,
    GroupType           TEXT NOT NULL,
    GroupKey            TEXT NOT NULL UNIQUE,
    DisplayName         TEXT NOT NULL,
    FOREIGN KEY (CompetitionSeasonID) REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (ParentGroupID) REFERENCES CompetitionGroups(CompetitionGroupID)
);

CREATE TABLE Teams
(
    TeamID   INTEGER PRIMARY KEY,
    TeamKey  TEXT NOT NULL UNIQUE,
    City     TEXT NOT NULL,
    Nickname TEXT NOT NULL
);

CREATE TABLE TeamSeasons
(
    TeamSeasonID       INTEGER PRIMARY KEY,
    TeamID             INTEGER NOT NULL,
    CompetitionSeasonID INTEGER NOT NULL,
    ConferenceGroupID  INTEGER,
    DivisionGroupID    INTEGER,
    HomeVenueID        INTEGER,
    TeamSeasonKey      TEXT NOT NULL UNIQUE,
    SourceStadiumLabel TEXT,
    StadiumType        TEXT,
    TurfType           TEXT,
    UNIQUE (TeamID, CompetitionSeasonID),
    FOREIGN KEY (TeamID) REFERENCES Teams(TeamID),
    FOREIGN KEY (CompetitionSeasonID) REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (ConferenceGroupID) REFERENCES CompetitionGroups(CompetitionGroupID),
    FOREIGN KEY (DivisionGroupID) REFERENCES CompetitionGroups(CompetitionGroupID),
    FOREIGN KEY (HomeVenueID) REFERENCES Venues(VenueID)
);

CREATE TABLE Players
(
    PlayerID        INTEGER PRIMARY KEY,
    SourceLocalKey  TEXT NOT NULL UNIQUE,
    TeamSeasonID    INTEGER NOT NULL,
    FirstName       TEXT NOT NULL,
    LastName        TEXT NOT NULL,
    Jersey          INTEGER NOT NULL,
    Position        TEXT NOT NULL,
    InjuryRating    INTEGER,
    Age             INTEGER,
    HeightInches    INTEGER,
    WeightPounds    INTEGER,
    SourceCsvLine   INTEGER,
    FOREIGN KEY (TeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID)
);

CREATE TABLE PlayerRatingProfiles
(
    PlayerRatingProfileID INTEGER PRIMARY KEY,
    PlayerID               INTEGER NOT NULL UNIQUE,
    RulesetID              INTEGER NOT NULL,
    CardType               TEXT NOT NULL,
    SourceRecordType       TEXT,
    SourceCsvLine          INTEGER,
    FOREIGN KEY (PlayerID) REFERENCES Players(PlayerID),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE TABLE PlayerRatings
(
    PlayerRatingProfileID INTEGER NOT NULL,
    RatingKey             TEXT NOT NULL,
    RatingValue           INTEGER NOT NULL,
    PRIMARY KEY (PlayerRatingProfileID, RatingKey),
    FOREIGN KEY (PlayerRatingProfileID)
        REFERENCES PlayerRatingProfiles(PlayerRatingProfileID)
);

CREATE TABLE TeamSeasonRatings
(
    TeamSeasonID INTEGER NOT NULL,
    RulesetID    INTEGER NOT NULL,
    RatingKey    TEXT NOT NULL,
    RatingValue  INTEGER NOT NULL,
    PRIMARY KEY (TeamSeasonID, RulesetID, RatingKey),
    FOREIGN KEY (TeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE TABLE BasicUnitRatings
(
    TeamSeasonID INTEGER NOT NULL,
    RulesetID    INTEGER NOT NULL,
    Side         TEXT NOT NULL,
    SkillKey     TEXT NOT NULL,
    UnitKey      TEXT NOT NULL,
    RatingValue  INTEGER NOT NULL,
    PRIMARY KEY (TeamSeasonID, RulesetID, Side, SkillKey, UnitKey),
    FOREIGN KEY (TeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE TABLE ScheduleGames
(
    SourceGameID       TEXT PRIMARY KEY,
    CompetitionSeasonID INTEGER NOT NULL,
    Stage              TEXT NOT NULL,
    WeekNumber         INTEGER,
    GameDate           TEXT NOT NULL,
    DayLabel           TEXT,
    TimeLabel          TEXT,
    AwayTeamSeasonID   INTEGER NOT NULL,
    HomeTeamSeasonID   INTEGER NOT NULL,
    Site               TEXT NOT NULL,
    FOREIGN KEY (CompetitionSeasonID) REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (AwayTeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (HomeTeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID)
);

CREATE TABLE ResultCodes
(
    ResultCodeID  INTEGER PRIMARY KEY,
    RulesetID     INTEGER NOT NULL,
    Context       TEXT NOT NULL,
    LegacyNumeric INTEGER,
    Code          TEXT NOT NULL,
    Semantic      TEXT NOT NULL,
    UNIQUE (RulesetID, Context, Code),
    UNIQUE (RulesetID, Context, LegacyNumeric),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE TABLE OffensivePlays
(
    OffensivePlayID INTEGER PRIMARY KEY,
    RulesetID       INTEGER NOT NULL,
    PlayKey         TEXT NOT NULL,
    Name            TEXT NOT NULL,
    PlayType        TEXT NOT NULL,
    SourceCsvLine   INTEGER,
    UNIQUE (RulesetID, PlayKey),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE TABLE OffensivePlayAdjustments
(
    OffensivePlayID INTEGER NOT NULL,
    AdjustmentKey   TEXT NOT NULL,
    AdjustmentValue INTEGER NOT NULL,
    PRIMARY KEY (OffensivePlayID, AdjustmentKey),
    FOREIGN KEY (OffensivePlayID) REFERENCES OffensivePlays(OffensivePlayID)
);

CREATE TABLE OffensivePlayMatchups
(
    MatchupID          INTEGER PRIMARY KEY,
    OffensivePlayID    INTEGER NOT NULL,
    WhiteD20           INTEGER NOT NULL,
    OffensivePosition  TEXT NOT NULL,
    OffensiveSkill     TEXT NOT NULL,
    DefensivePosition  TEXT NOT NULL,
    DefensiveSkill     TEXT NOT NULL,
    SourceCsvLine      INTEGER,
    UNIQUE (OffensivePlayID, WhiteD20),
    FOREIGN KEY (OffensivePlayID) REFERENCES OffensivePlays(OffensivePlayID)
);

CREATE TABLE OffensivePlayOutcomes
(
    MatchupID   INTEGER NOT NULL,
    D10         INTEGER NOT NULL CHECK (D10 BETWEEN 0 AND 9),
    ResultToken TEXT NOT NULL,
    PRIMARY KEY (MatchupID, D10),
    FOREIGN KEY (MatchupID) REFERENCES OffensivePlayMatchups(MatchupID)
);

CREATE TABLE DefensivePlays
(
    DefensivePlayID    INTEGER PRIMARY KEY,
    RulesetID          INTEGER NOT NULL,
    DefenseKey         TEXT NOT NULL,
    Name               TEXT NOT NULL,
    SourceCsvLine      INTEGER,
    MinMenOnLine       INTEGER NOT NULL,
    PassRush           TEXT NOT NULL,
    PassCoverage       TEXT NOT NULL,
    PlayActionModifier INTEGER NOT NULL,
    UNIQUE (RulesetID, DefenseKey),
    FOREIGN KEY (RulesetID) REFERENCES Rulesets(RulesetID)
);

CREATE INDEX IX_TeamSeasons_CompetitionSeason
    ON TeamSeasons(CompetitionSeasonID);

CREATE INDEX IX_Players_TeamSeason
    ON Players(TeamSeasonID);

CREATE INDEX IX_ScheduleGames_CompetitionSeason
    ON ScheduleGames(CompetitionSeasonID);

CREATE INDEX IX_ScheduleGames_AwayTeam
    ON ScheduleGames(AwayTeamSeasonID);

CREATE INDEX IX_ScheduleGames_HomeTeam
    ON ScheduleGames(HomeTeamSeasonID);

CREATE INDEX IX_OffensivePlayMatchups_Play
    ON OffensivePlayMatchups(OffensivePlayID);
)SQL");
    }

    void insertMetadata(sqlite3* db,
        BuildContext& context,
        const std::string& key,
        const std::string& value)
    {
        Statement statement(
            db,
            "INSERT INTO CoreMetadata(MetadataKey, MetadataValue) VALUES(?, ?);");

        bindText(statement.get(), 1, key);
        bindText(statement.get(), 2, value);
        statement.stepDone(db);
        context.inserted();
    }

    sqlite3_int64 insertRuleset(BuildContext& context,
        const std::string& ruleset)
    {
        Statement statement(
            context.db,
            "INSERT INTO Rulesets(RulesetKey) VALUES(?);");

        bindText(statement.get(), 1, ruleset);
        statement.stepDone(context.db);
        context.inserted();

        return lastInsertId(context.db);
    }

    void registerSourceFile(BuildContext& context,
        const std::filesystem::path& file,
        const std::string& family,
        const std::string& status,
        std::uint64_t recordsImported)
    {
        std::error_code ec;
        std::filesystem::path relative =
            std::filesystem::relative(file, context.sourceRoot, ec);

        if (ec)
        {
            relative = file.filename();
        }

        Statement statement(
            context.db,
            R"SQL(
INSERT INTO SourceFiles(RelativePath, DataFamily, Status, RecordsImported)
VALUES(?, ?, ?, ?)
ON CONFLICT(RelativePath) DO UPDATE SET
    DataFamily = excluded.DataFamily,
    Status = excluded.Status,
    RecordsImported = excluded.RecordsImported;
)SQL");

        bindText(statement.get(), 1, pathUtf8(relative));
        bindText(statement.get(), 2, family);
        bindText(statement.get(), 3, status);
        bindInt64(
            statement.get(),
            4,
            static_cast<sqlite3_int64>(recordsImported));

        statement.stepDone(context.db);
        context.inserted();
    }

    std::uint64_t loadVenues(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct VenueData
        {
            std::string key;
            std::string name;
            std::string city;
            std::string state;
            std::string latitude;
            std::string longitude;
            std::string timezone;
            std::string surface;
            std::string stadiumType;
            std::string capacity;
            std::size_t beginLine = 0;
        };

        VenueData current;
        bool inVenue = false;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO Venues
(
    VenueKey, Name, City, State, LatitudeText, LongitudeText,
    TimeZone, Surface, StadiumType, CapacityText
)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "VENUE_BEGIN")
            {
                if (inVenue)
                {
                    throw BuildException(
                        "Nested VENUE_BEGIN is not valid.",
                        file,
                        tag.line);
                }

                current = VenueData{};
                current.beginLine = tag.line;
                inVenue = true;
                continue;
            }

            if (!inVenue)
            {
                continue;
            }

            if (tag.tag == "VENUE_END")
            {
                if (current.key.empty() || current.name.empty())
                {
                    throw BuildException(
                        "Venue requires VENUE_KEY and VENUENAME.",
                        file,
                        current.beginLine);
                }

                insert.reset();
                bindText(insert.get(), 1, current.key);
                bindText(insert.get(), 2, current.name);
                bindText(insert.get(), 3, current.city);
                bindText(insert.get(), 4, current.state);
                bindText(insert.get(), 5, current.latitude);
                bindText(insert.get(), 6, current.longitude);
                bindText(insert.get(), 7, current.timezone);
                bindText(insert.get(), 8, current.surface);
                bindText(insert.get(), 9, current.stadiumType);
                bindText(insert.get(), 10, current.capacity);
                insert.stepDone(context.db);

                const sqlite3_int64 id = lastInsertId(context.db);
                if (!context.venueByKey.emplace(current.key, id).second)
                {
                    throw BuildException(
                        "Duplicate VENUE_KEY: " + current.key,
                        file,
                        tag.line);
                }

                ++count;
                context.inserted();
                inVenue = false;
                continue;
            }

            if (tag.tag == "VENUE_KEY") current.key = tag.value;
            else if (tag.tag == "VENUENAME") current.name = tag.value;
            else if (tag.tag == "CITY") current.city = tag.value;
            else if (tag.tag == "STATE") current.state = tag.value;
            else if (tag.tag == "LATITUDE") current.latitude = tag.value;
            else if (tag.tag == "LONGITUDE") current.longitude = tag.value;
            else if (tag.tag == "TIMEZONE") current.timezone = tag.value;
            else if (tag.tag == "SURFACE") current.surface = tag.value;
            else if (tag.tag == "STADIUM_TYPE") current.stadiumType = tag.value;
            else if (tag.tag == "CAPACITY") current.capacity = tag.value;
        }

        if (inVenue)
        {
            throw BuildException("Unclosed VENUE block.", file, current.beginLine);
        }

        return count;
    }

    std::uint64_t loadVenueAliases(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct AliasData
        {
            std::string sourceLabel;
            std::string venueName;
            std::string venueKey;
            std::string provenance;
            std::size_t beginLine = 0;
        };

        AliasData current;
        bool inAlias = false;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO VenueAliases
(
    SourceStadiumLabel, VenueID, VenueName, Provenance
)
VALUES(?, ?, ?, ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "VENUE_ALIAS_BEGIN")
            {
                current = AliasData{};
                current.beginLine = tag.line;
                inAlias = true;
                continue;
            }

            if (!inAlias)
            {
                continue;
            }

            if (tag.tag == "VENUE_ALIAS_END")
            {
                const auto venueIt = context.venueByKey.find(current.venueKey);
                if (venueIt == context.venueByKey.end())
                {
                    throw BuildException(
                        "Venue alias references unknown VENUE_KEY: " +
                        current.venueKey,
                        file,
                        current.beginLine);
                }

                insert.reset();
                bindText(insert.get(), 1, current.sourceLabel);
                bindInt64(insert.get(), 2, venueIt->second);
                bindText(insert.get(), 3, current.venueName);
                bindText(insert.get(), 4, current.provenance);
                insert.stepDone(context.db);

                ++count;
                context.inserted();
                inAlias = false;
                continue;
            }

            if (tag.tag == "SOURCE_STADIUM_LABEL") current.sourceLabel = tag.value;
            else if (tag.tag == "VENUE_NAME") current.venueName = tag.value;
            else if (tag.tag == "VENUE_KEY") current.venueKey = tag.value;
            else if (tag.tag == "PROVENANCE") current.provenance = tag.value;
        }

        return count;
    }

    std::uint64_t loadLeagueStructure(BuildContext& context,
        const std::filesystem::path& file,
        int requestedSeason)
    {
        const auto tags = readTaggedFile(file);

        struct TeamData
        {
            std::string city;
            std::string nickname;
            std::string stadium;
            std::string stadiumType;
            std::string turfType;
            std::string teamKey;
            std::string teamSeasonKey;
            std::string homeVenueKey;
            std::size_t beginLine = 0;
        };

        std::string league;
        std::string conference;
        std::string division;

        sqlite3_int64 currentConferenceId = 0;
        sqlite3_int64 currentDivisionId = 0;

        bool inTeam = false;
        TeamData team;
        std::uint64_t teamCount = 0;

        auto ensureCompetition = [&]()
            {
                if (context.competitionId != 0)
                {
                    return;
                }

                if (league.empty())
                {
                    throw BuildException(
                        "LEAGUE must be defined before team records.",
                        file);
                }

                Statement competitionInsert(
                    context.db,
                    "INSERT INTO Competitions(CompetitionKey, DisplayName) VALUES(?, ?);");

                bindText(competitionInsert.get(), 1, league);
                bindText(competitionInsert.get(), 2, league);
                competitionInsert.stepDone(context.db);
                context.competitionId = lastInsertId(context.db);
                context.inserted();

                Statement seasonInsert(
                    context.db,
                    R"SQL(
INSERT INTO CompetitionSeasons(CompetitionID, SeasonYear)
VALUES(?, ?);
)SQL");

                bindInt64(seasonInsert.get(), 1, context.competitionId);
                bindInt(seasonInsert.get(), 2, requestedSeason);
                seasonInsert.stepDone(context.db);
                context.competitionSeasonId = lastInsertId(context.db);
                context.inserted();
            };

        Statement groupInsert(
            context.db,
            R"SQL(
INSERT INTO CompetitionGroups
(
    CompetitionSeasonID, ParentGroupID, GroupType, GroupKey, DisplayName
)
VALUES(?, ?, ?, ?, ?);
)SQL");

        Statement teamInsert(
            context.db,
            "INSERT INTO Teams(TeamKey, City, Nickname) VALUES(?, ?, ?);");

        Statement teamSeasonInsert(
            context.db,
            R"SQL(
INSERT INTO TeamSeasons
(
    TeamID,
    CompetitionSeasonID,
    ConferenceGroupID,
    DivisionGroupID,
    HomeVenueID,
    TeamSeasonKey,
    SourceStadiumLabel,
    StadiumType,
    TurfType
)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL");

        int fileSeason = 0;

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "SEASON" && !inTeam)
            {
                fileSeason = parseInt(tag.value, file, tag.line, "SEASON");
                if (fileSeason != requestedSeason)
                {
                    throw BuildException(
                        "League source season does not match build request.",
                        file,
                        tag.line);
                }
                continue;
            }

            if (tag.tag == "LEAGUE")
            {
                league = tag.value;
                ensureCompetition();
                continue;
            }

            if (tag.tag == "CONFERENCE")
            {
                ensureCompetition();

                conference = tag.value;
                const std::string key =
                    league + "_" + std::to_string(requestedSeason) +
                    "_CONFERENCE_" + conference;

                groupInsert.reset();
                bindInt64(
                    groupInsert.get(),
                    1,
                    context.competitionSeasonId);
                bindNull(groupInsert.get(), 2);
                bindText(groupInsert.get(), 3, "CONFERENCE");
                bindText(groupInsert.get(), 4, key);
                bindText(groupInsert.get(), 5, conference);
                groupInsert.stepDone(context.db);

                currentConferenceId = lastInsertId(context.db);
                context.groupByKey[key] = currentConferenceId;
                currentDivisionId = 0;
                context.inserted();
                continue;
            }

            if (tag.tag == "DIVISION")
            {
                if (currentConferenceId == 0)
                {
                    throw BuildException(
                        "DIVISION encountered before CONFERENCE.",
                        file,
                        tag.line);
                }

                division = tag.value;
                const std::string key =
                    league + "_" + std::to_string(requestedSeason) +
                    "_DIVISION_" + conference + "_" + division;

                groupInsert.reset();
                bindInt64(
                    groupInsert.get(),
                    1,
                    context.competitionSeasonId);
                bindInt64(groupInsert.get(), 2, currentConferenceId);
                bindText(groupInsert.get(), 3, "DIVISION");
                bindText(groupInsert.get(), 4, key);
                bindText(groupInsert.get(), 5, division);
                groupInsert.stepDone(context.db);

                currentDivisionId = lastInsertId(context.db);
                context.groupByKey[key] = currentDivisionId;
                context.inserted();
                continue;
            }

            if (tag.tag == "TEAM_BEGIN")
            {
                if (currentDivisionId == 0)
                {
                    throw BuildException(
                        "TEAM_BEGIN encountered outside a division.",
                        file,
                        tag.line);
                }

                inTeam = true;
                team = TeamData{};
                team.beginLine = tag.line;
                continue;
            }

            if (!inTeam)
            {
                continue;
            }

            if (tag.tag == "TEAM_END")
            {
                if (team.teamKey.empty() ||
                    team.teamSeasonKey.empty() ||
                    team.city.empty() ||
                    team.nickname.empty())
                {
                    throw BuildException(
                        "Team block is missing required identity tags.",
                        file,
                        team.beginLine);
                }

                sqlite3_int64 homeVenueId = 0;
                if (!team.homeVenueKey.empty())
                {
                    const auto venueIt =
                        context.venueByKey.find(team.homeVenueKey);

                    if (venueIt == context.venueByKey.end())
                    {
                        throw BuildException(
                            "TEAM references unknown HOME_VENUE_KEY: " +
                            team.homeVenueKey,
                            file,
                            team.beginLine);
                    }

                    homeVenueId = venueIt->second;
                }

                teamInsert.reset();
                bindText(teamInsert.get(), 1, team.teamKey);
                bindText(teamInsert.get(), 2, team.city);
                bindText(teamInsert.get(), 3, team.nickname);
                teamInsert.stepDone(context.db);

                const sqlite3_int64 teamId = lastInsertId(context.db);
                context.teamByKey[team.teamKey] = teamId;
                context.inserted();

                teamSeasonInsert.reset();
                bindInt64(teamSeasonInsert.get(), 1, teamId);
                bindInt64(
                    teamSeasonInsert.get(),
                    2,
                    context.competitionSeasonId);
                bindInt64(
                    teamSeasonInsert.get(),
                    3,
                    currentConferenceId);
                bindInt64(
                    teamSeasonInsert.get(),
                    4,
                    currentDivisionId);

                if (homeVenueId != 0)
                {
                    bindInt64(
                        teamSeasonInsert.get(),
                        5,
                        homeVenueId);
                }
                else
                {
                    bindNull(teamSeasonInsert.get(), 5);
                }

                bindText(
                    teamSeasonInsert.get(),
                    6,
                    team.teamSeasonKey);
                bindText(
                    teamSeasonInsert.get(),
                    7,
                    team.stadium);
                bindText(
                    teamSeasonInsert.get(),
                    8,
                    team.stadiumType);
                bindText(
                    teamSeasonInsert.get(),
                    9,
                    team.turfType);

                teamSeasonInsert.stepDone(context.db);

                const sqlite3_int64 teamSeasonId =
                    lastInsertId(context.db);

                context.teamSeasonByKey[team.teamSeasonKey] =
                    teamSeasonId;

                const std::string sourceName =
                    sourceTeamNameKey(team.city, team.nickname);

                if (!context.teamSeasonBySourceName
                    .emplace(sourceName, teamSeasonId)
                    .second)
                {
                    throw BuildException(
                        "Duplicate team city/nickname mapping: " +
                        team.city + " " + team.nickname,
                        file,
                        team.beginLine);
                }

                ++teamCount;
                context.inserted();
                inTeam = false;
                continue;
            }

            if (tag.tag == "CITY") team.city = tag.value;
            else if (tag.tag == "TEAM") team.nickname = tag.value;
            else if (tag.tag == "STADIUM") team.stadium = tag.value;
            else if (tag.tag == "STADIUM_TYPE") team.stadiumType = tag.value;
            else if (tag.tag == "TURF_TYPE") team.turfType = tag.value;
            else if (tag.tag == "TEAM_KEY") team.teamKey = tag.value;
            else if (tag.tag == "TEAM_SEASON_KEY") team.teamSeasonKey = tag.value;
            else if (tag.tag == "HOME_VENUE_KEY") team.homeVenueKey = tag.value;
        }

        if (fileSeason == 0)
        {
            throw BuildException("League source is missing SEASON.", file);
        }

        if (teamCount == 0)
        {
            throw BuildException("No teams were loaded.", file);
        }

        return teamCount;
    }

    std::uint64_t loadPlayers(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct PlayerData
        {
            std::string sourceLocalKey;
            std::string firstName;
            std::string lastName;
            std::string position;
            std::string sourceTeamCity;
            std::string sourceTeamNickname;
            int jersey = 0;
            int injury = 0;
            int age = 0;
            int height = 0;
            int weight = 0;
            int sourceCsvLine = 0;
            std::size_t beginLine = 0;
        };

        struct ProfileData
        {
            std::string ruleset;
            std::string cardType;
            std::string sourceRecordType;
            int sourceCsvLine = 0;
            std::vector<std::pair<std::string, int>> ratings;
        };

        PlayerData player;
        ProfileData profile;
        bool inPlayer = false;
        bool inProfile = false;
        std::uint64_t playerCount = 0;

        Statement playerInsert(
            context.db,
            R"SQL(
INSERT INTO Players
(
    SourceLocalKey, TeamSeasonID, FirstName, LastName, Jersey,
    Position, InjuryRating, Age, HeightInches, WeightPounds, SourceCsvLine
)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL");

        Statement profileInsert(
            context.db,
            R"SQL(
INSERT INTO PlayerRatingProfiles
(
    PlayerID, RulesetID, CardType, SourceRecordType, SourceCsvLine
)
VALUES(?, ?, ?, ?, ?);
)SQL");

        Statement ratingInsert(
            context.db,
            R"SQL(
INSERT INTO PlayerRatings
(
    PlayerRatingProfileID, RatingKey, RatingValue
)
VALUES(?, ?, ?);
)SQL");

        auto insertPlayer = [&](std::size_t sourceLine)
            {
                const std::string sourceTeam =
                    sourceTeamNameKey(
                        player.sourceTeamCity,
                        player.sourceTeamNickname);

                const auto teamIt =
                    context.teamSeasonBySourceName.find(sourceTeam);

                if (teamIt == context.teamSeasonBySourceName.end())
                {
                    throw BuildException(
                        "Player references unknown source team: " +
                        player.sourceTeamCity + " " +
                        player.sourceTeamNickname,
                        file,
                        sourceLine);
                }

                if (profile.ruleset.empty() ||
                    profile.ruleset != "FirstEdition")
                {
                    throw BuildException(
                        "Player rating profile RULESET is missing or unsupported: " +
                        profile.ruleset,
                        file,
                        sourceLine);
                }

                playerInsert.reset();
                bindText(playerInsert.get(), 1, player.sourceLocalKey);
                bindInt64(playerInsert.get(), 2, teamIt->second);
                bindText(playerInsert.get(), 3, player.firstName);
                bindText(playerInsert.get(), 4, player.lastName);
                bindInt(playerInsert.get(), 5, player.jersey);
                bindText(playerInsert.get(), 6, player.position);
                bindInt(playerInsert.get(), 7, player.injury);
                bindInt(playerInsert.get(), 8, player.age);
                bindInt(playerInsert.get(), 9, player.height);
                bindInt(playerInsert.get(), 10, player.weight);

                if (player.sourceCsvLine > 0)
                {
                    bindInt(
                        playerInsert.get(),
                        11,
                        player.sourceCsvLine);
                }
                else
                {
                    bindNull(playerInsert.get(), 11);
                }

                playerInsert.stepDone(context.db);
                const sqlite3_int64 playerId =
                    lastInsertId(context.db);
                context.inserted();

                profileInsert.reset();
                bindInt64(profileInsert.get(), 1, playerId);
                bindInt64(
                    profileInsert.get(),
                    2,
                    context.rulesetId);
                bindText(profileInsert.get(), 3, profile.cardType);
                bindText(
                    profileInsert.get(),
                    4,
                    profile.sourceRecordType);

                if (profile.sourceCsvLine > 0)
                {
                    bindInt(
                        profileInsert.get(),
                        5,
                        profile.sourceCsvLine);
                }
                else
                {
                    bindNull(profileInsert.get(), 5);
                }

                profileInsert.stepDone(context.db);
                const sqlite3_int64 profileId =
                    lastInsertId(context.db);
                context.inserted();

                for (const auto& [ratingKey, ratingValue] :
                    profile.ratings)
                {
                    ratingInsert.reset();
                    bindInt64(ratingInsert.get(), 1, profileId);
                    bindText(ratingInsert.get(), 2, ratingKey);
                    bindInt(ratingInsert.get(), 3, ratingValue);
                    ratingInsert.stepDone(context.db);
                    context.inserted();
                }

                ++playerCount;
            };

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "PLAYER_BEGIN")
            {
                if (inPlayer)
                {
                    throw BuildException(
                        "Nested PLAYER_BEGIN is not valid.",
                        file,
                        tag.line);
                }

                player = PlayerData{};
                profile = ProfileData{};
                player.beginLine = tag.line;
                inPlayer = true;
                continue;
            }

            if (!inPlayer)
            {
                continue;
            }

            if (tag.tag == "RATING_PROFILE_BEGIN")
            {
                if (inProfile)
                {
                    throw BuildException(
                        "Nested RATING_PROFILE_BEGIN is not valid.",
                        file,
                        tag.line);
                }

                profile = ProfileData{};
                inProfile = true;
                continue;
            }

            if (tag.tag == "RATING_PROFILE_END")
            {
                inProfile = false;
                continue;
            }

            if (tag.tag == "PLAYER_END")
            {
                if (inProfile)
                {
                    throw BuildException(
                        "PLAYER_END encountered before RATING_PROFILE_END.",
                        file,
                        tag.line);
                }

                if (player.sourceLocalKey.empty() ||
                    player.firstName.empty() ||
                    player.lastName.empty() ||
                    player.position.empty())
                {
                    throw BuildException(
                        "Player block is missing required identity data.",
                        file,
                        player.beginLine);
                }

                insertPlayer(player.beginLine);
                inPlayer = false;
                continue;
            }

            if (inProfile)
            {
                if (tag.tag == "RULESET")
                {
                    profile.ruleset = tag.value;
                }
                else if (tag.tag == "CARD_TYPE")
                {
                    profile.cardType = tag.value;
                }
                else if (tag.tag == "SOURCE_RECORD_TYPE")
                {
                    profile.sourceRecordType = tag.value;
                }
                else if (tag.tag == "SOURCE_CSV_LINE")
                {
                    profile.sourceCsvLine =
                        parseInt(
                            tag.value,
                            file,
                            tag.line,
                            "SOURCE_CSV_LINE");
                }
                else
                {
                    profile.ratings.emplace_back(
                        tag.tag,
                        parseInt(
                            tag.value,
                            file,
                            tag.line,
                            tag.tag.c_str()));
                }

                continue;
            }

            if (tag.tag == "SOURCE_LOCAL_KEY") player.sourceLocalKey = tag.value;
            else if (tag.tag == "FIRST_NAME") player.firstName = tag.value;
            else if (tag.tag == "LAST_NAME") player.lastName = tag.value;
            else if (tag.tag == "POSITION") player.position = tag.value;
            else if (tag.tag == "SOURCE_TEAM_CITY") player.sourceTeamCity = tag.value;
            else if (tag.tag == "SOURCE_TEAM_NICKNAME") player.sourceTeamNickname = tag.value;
            else if (tag.tag == "JERSEY")
                player.jersey = parseInt(tag.value, file, tag.line, "JERSEY");
            else if (tag.tag == "INJURY")
                player.injury = parseInt(tag.value, file, tag.line, "INJURY");
            else if (tag.tag == "AGE")
                player.age = parseInt(tag.value, file, tag.line, "AGE");
            else if (tag.tag == "HEIGHT_INCHES")
                player.height = parseInt(tag.value, file, tag.line, "HEIGHT_INCHES");
            else if (tag.tag == "WEIGHT_POUNDS")
                player.weight = parseInt(tag.value, file, tag.line, "WEIGHT_POUNDS");
            else if (tag.tag == "SOURCE_CSV_LINE")
                player.sourceCsvLine =
                parseInt(
                    tag.value,
                    file,
                    tag.line,
                    "SOURCE_CSV_LINE");
        }

        return playerCount;
    }

    std::uint64_t loadBasicOffensiveRatings(
        BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        sqlite3_int64 teamSeasonId = 0;
        std::string skillKey;
        bool inRating = false;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO BasicUnitRatings
(
    TeamSeasonID, RulesetID, Side, SkillKey, UnitKey, RatingValue
)
VALUES(?, ?, 'OFFENSE', ?, 'OL', ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "TEAM_SEASON_KEY")
            {
                const auto it =
                    context.teamSeasonByKey.find(tag.value);

                if (it == context.teamSeasonByKey.end())
                {
                    throw BuildException(
                        "Offensive unit rating references unknown TEAM_SEASON_KEY: " +
                        tag.value,
                        file,
                        tag.line);
                }

                teamSeasonId = it->second;
            }
            else if (tag.tag == "OFFENSIVE_UNIT_RATING_BEGIN")
            {
                skillKey.clear();
                inRating = true;
            }
            else if (tag.tag == "OFFENSIVE_UNIT_RATING_END")
            {
                inRating = false;
            }
            else if (inRating && tag.tag == "SKILL_KEY")
            {
                skillKey = tag.value;
            }
            else if (inRating && tag.tag == "OL")
            {
                if (teamSeasonId == 0 || skillKey.empty())
                {
                    throw BuildException(
                        "Incomplete offensive unit-rating block.",
                        file,
                        tag.line);
                }

                insert.reset();
                bindInt64(insert.get(), 1, teamSeasonId);
                bindInt64(insert.get(), 2, context.rulesetId);
                bindText(insert.get(), 3, skillKey);
                bindInt(
                    insert.get(),
                    4,
                    parseInt(tag.value, file, tag.line, "OL"));
                insert.stepDone(context.db);

                ++count;
                context.inserted();
            }
        }

        return count;
    }

    std::uint64_t loadBasicDefensiveRatings(
        BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        sqlite3_int64 teamSeasonId = 0;
        std::string skillKey;
        bool inTeam = false;
        bool inRating = false;
        std::uint64_t count = 0;

        Statement ratingInsert(
            context.db,
            R"SQL(
INSERT INTO BasicUnitRatings
(
    TeamSeasonID, RulesetID, Side, SkillKey, UnitKey, RatingValue
)
VALUES(?, ?, 'DEFENSE', ?, ?, ?);
)SQL");

        Statement teamRatingInsert(
            context.db,
            R"SQL(
INSERT INTO TeamSeasonRatings
(
    TeamSeasonID, RulesetID, RatingKey, RatingValue
)
VALUES(?, ?, 'DEFENSIVE_PENALTY', ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "BASIC_DEFENSIVE_TEAM_BEGIN")
            {
                inTeam = true;
                teamSeasonId = 0;
                continue;
            }

            if (tag.tag == "BASIC_DEFENSIVE_TEAM_END")
            {
                inTeam = false;
                teamSeasonId = 0;
                continue;
            }

            if (!inTeam)
            {
                continue;
            }

            if (tag.tag == "TEAM_SEASON_KEY")
            {
                const auto it =
                    context.teamSeasonByKey.find(tag.value);

                if (it == context.teamSeasonByKey.end())
                {
                    throw BuildException(
                        "Defensive unit rating references unknown TEAM_SEASON_KEY: " +
                        tag.value,
                        file,
                        tag.line);
                }

                teamSeasonId = it->second;
            }
            else if (tag.tag == "DEFENSIVE_PENALTY_RATING")
            {
                if (teamSeasonId == 0)
                {
                    throw BuildException(
                        "DEFENSIVE_PENALTY_RATING appears before TEAM_SEASON_KEY.",
                        file,
                        tag.line);
                }

                teamRatingInsert.reset();
                bindInt64(
                    teamRatingInsert.get(),
                    1,
                    teamSeasonId);
                bindInt64(
                    teamRatingInsert.get(),
                    2,
                    context.rulesetId);
                bindInt(
                    teamRatingInsert.get(),
                    3,
                    parseInt(
                        tag.value,
                        file,
                        tag.line,
                        "DEFENSIVE_PENALTY_RATING"));
                teamRatingInsert.stepDone(context.db);
                context.inserted();
            }
            else if (tag.tag == "DEFENSIVE_UNIT_RATING_BEGIN")
            {
                skillKey.clear();
                inRating = true;
            }
            else if (tag.tag == "DEFENSIVE_UNIT_RATING_END")
            {
                inRating = false;
            }
            else if (inRating && tag.tag == "SKILL_KEY")
            {
                skillKey = tag.value;
            }
            else if (inRating &&
                (tag.tag == "DL" ||
                    tag.tag == "LB" ||
                    tag.tag == "DB"))
            {
                if (teamSeasonId == 0 || skillKey.empty())
                {
                    throw BuildException(
                        "Incomplete defensive unit-rating block.",
                        file,
                        tag.line);
                }

                ratingInsert.reset();
                bindInt64(ratingInsert.get(), 1, teamSeasonId);
                bindInt64(ratingInsert.get(), 2, context.rulesetId);
                bindText(ratingInsert.get(), 3, skillKey);
                bindText(ratingInsert.get(), 4, tag.tag);
                bindInt(
                    ratingInsert.get(),
                    5,
                    parseInt(
                        tag.value,
                        file,
                        tag.line,
                        tag.tag.c_str()));
                ratingInsert.stepDone(context.db);

                ++count;
                context.inserted();
            }
        }

        return count;
    }

    std::uint64_t loadSchedule(BuildContext& context,
        const std::filesystem::path& file,
        int requestedSeason)
    {
        const auto tags = readTaggedFile(file);

        struct GameData
        {
            std::string gameId;
            std::string date;
            std::string day;
            std::string time;
            std::string awayTeamSeasonKey;
            std::string homeTeamSeasonKey;
            std::string site;
            std::size_t beginLine = 0;
        };

        int currentWeek = 0;
        bool postseason = false;
        bool inGame = false;
        GameData game;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO ScheduleGames
(
    SourceGameID,
    CompetitionSeasonID,
    Stage,
    WeekNumber,
    GameDate,
    DayLabel,
    TimeLabel,
    AwayTeamSeasonID,
    HomeTeamSeasonID,
    Site
)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "SEASON")
            {
                const int season =
                    parseInt(tag.value, file, tag.line, "SEASON");

                if (season != requestedSeason)
                {
                    throw BuildException(
                        "Schedule season does not match build request.",
                        file,
                        tag.line);
                }
            }
            else if (tag.tag == "WEEK")
            {
                currentWeek =
                    parseInt(tag.value, file, tag.line, "WEEK");
            }
            else if (tag.tag == "POSTSEASON_BEGIN")
            {
                postseason = true;
                currentWeek = 0;
            }
            else if (tag.tag == "POSTSEASON_END")
            {
                postseason = false;
            }
            else if (tag.tag == "GAME_BEGIN")
            {
                game = GameData{};
                game.beginLine = tag.line;
                inGame = true;
            }
            else if (tag.tag == "GAME_END")
            {
                if (!inGame)
                {
                    throw BuildException(
                        "GAME_END without GAME_BEGIN.",
                        file,
                        tag.line);
                }

                const auto awayIt =
                    context.teamSeasonByKey.find(
                        game.awayTeamSeasonKey);

                const auto homeIt =
                    context.teamSeasonByKey.find(
                        game.homeTeamSeasonKey);

                if (awayIt == context.teamSeasonByKey.end())
                {
                    throw BuildException(
                        "Schedule references unknown away TEAM_SEASON_KEY: " +
                        game.awayTeamSeasonKey,
                        file,
                        game.beginLine);
                }

                if (homeIt == context.teamSeasonByKey.end())
                {
                    throw BuildException(
                        "Schedule references unknown home TEAM_SEASON_KEY: " +
                        game.homeTeamSeasonKey,
                        file,
                        game.beginLine);
                }

                insert.reset();
                bindText(insert.get(), 1, game.gameId);
                bindInt64(
                    insert.get(),
                    2,
                    context.competitionSeasonId);
                bindText(
                    insert.get(),
                    3,
                    postseason ? "POSTSEASON" : "REGULAR");

                if (!postseason)
                {
                    bindInt(insert.get(), 4, currentWeek);
                }
                else
                {
                    bindNull(insert.get(), 4);
                }

                bindText(insert.get(), 5, game.date);
                bindText(insert.get(), 6, game.day);
                bindText(insert.get(), 7, game.time);
                bindInt64(insert.get(), 8, awayIt->second);
                bindInt64(insert.get(), 9, homeIt->second);
                bindText(insert.get(), 10, game.site);
                insert.stepDone(context.db);

                ++count;
                context.inserted();
                inGame = false;
            }
            else if (inGame)
            {
                if (tag.tag == "GAME_ID") game.gameId = tag.value;
                else if (tag.tag == "DATE") game.date = tag.value;
                else if (tag.tag == "DAY") game.day = tag.value;
                else if (tag.tag == "TIME") game.time = tag.value;
                else if (tag.tag == "AWAY_TEAM_SEASON_KEY")
                    game.awayTeamSeasonKey = tag.value;
                else if (tag.tag == "HOME_TEAM_SEASON_KEY")
                    game.homeTeamSeasonKey = tag.value;
                else if (tag.tag == "SITE") game.site = tag.value;
            }
        }

        return count;
    }

    std::uint64_t loadResultCodes(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct ResultCodeData
        {
            std::string context;
            int legacyNumeric = 0;
            bool hasLegacyNumeric = false;
            std::string code;
            std::string semantic;
            std::size_t beginLine = 0;
        };

        ResultCodeData current;
        bool inBlock = false;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO ResultCodes
(
    RulesetID, Context, LegacyNumeric, Code, Semantic
)
VALUES(?, ?, ?, ?, ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "RESULT_CODE_BEGIN")
            {
                current = ResultCodeData{};
                current.beginLine = tag.line;
                inBlock = true;
            }
            else if (tag.tag == "RESULT_CODE_END")
            {
                if (!inBlock ||
                    current.context.empty() ||
                    current.code.empty() ||
                    current.semantic.empty())
                {
                    throw BuildException(
                        "Incomplete RESULT_CODE block.",
                        file,
                        current.beginLine);
                }

                insert.reset();
                bindInt64(insert.get(), 1, context.rulesetId);
                bindText(insert.get(), 2, current.context);
                if (current.hasLegacyNumeric)
                    bindInt(insert.get(), 3, current.legacyNumeric);
                else
                    bindNull(insert.get(), 3);

                bindText(insert.get(), 4, current.code);
                bindText(insert.get(), 5, current.semantic);
                insert.stepDone(context.db);

                ++count;
                context.inserted();
                inBlock = false;
            }
            else if (inBlock)
            {
                if (tag.tag == "CONTEXT") current.context = tag.value;
                else if (tag.tag == "LEGACY_NUMERIC")
                {
                    if (!tag.value.empty())
                    {
                        current.legacyNumeric =
                            parseInt(
                                tag.value,
                                file,
                                tag.line,
                                "LEGACY_NUMERIC");
                        current.hasLegacyNumeric = true;
                    }
                }
                else if (tag.tag == "CODE") current.code = tag.value;
                else if (tag.tag == "SEMANTIC") current.semantic = tag.value;
            }
        }

        return count;
    }

    std::uint64_t loadOffensivePlays(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct MatchupData
        {
            int whiteD20 = 0;
            std::string offensivePosition;
            std::string offensiveSkill;
            std::string defensivePosition;
            std::string defensiveSkill;
            int sourceCsvLine = 0;
            std::string outcomes[10];
            bool hasOutcome[10] = {};
            std::size_t beginLine = 0;
        };

        struct PlayData
        {
            std::string playKey;
            std::string name;
            std::string playType;
            int sourceCsvLine = 0;
            std::vector<std::pair<std::string, int>> adjustments;
            std::vector<MatchupData> matchups;
            std::size_t beginLine = 0;
        };

        PlayData play;
        MatchupData matchup;
        bool inPlay = false;
        bool inMatchup = false;
        std::uint64_t totalRows = 0;

        Statement playInsert(
            context.db,
            R"SQL(
INSERT INTO OffensivePlays
(
    RulesetID, PlayKey, Name, PlayType, SourceCsvLine
)
VALUES(?, ?, ?, ?, ?);
)SQL");

        Statement adjustmentInsert(
            context.db,
            R"SQL(
INSERT INTO OffensivePlayAdjustments
(
    OffensivePlayID, AdjustmentKey, AdjustmentValue
)
VALUES(?, ?, ?);
)SQL");

        Statement matchupInsert(
            context.db,
            R"SQL(
INSERT INTO OffensivePlayMatchups
(
    OffensivePlayID, WhiteD20, OffensivePosition, OffensiveSkill,
    DefensivePosition, DefensiveSkill, SourceCsvLine
)
VALUES(?, ?, ?, ?, ?, ?, ?);
)SQL");

        Statement outcomeInsert(
            context.db,
            R"SQL(
INSERT INTO OffensivePlayOutcomes(MatchupID, D10, ResultToken)
VALUES(?, ?, ?);
)SQL");

        auto insertPlay = [&]()
            {
                if (play.playKey.empty() ||
                    play.name.empty() ||
                    play.playType.empty())
                {
                    throw BuildException(
                        "Offensive play is missing identity fields.",
                        file,
                        play.beginLine);
                }

                if (play.matchups.size() != 20)
                {
                    throw BuildException(
                        "Offensive play '" + play.playKey +
                        "' does not contain exactly 20 matchup rows.",
                        file,
                        play.beginLine);
                }

                playInsert.reset();
                bindInt64(playInsert.get(), 1, context.rulesetId);
                bindText(playInsert.get(), 2, play.playKey);
                bindText(playInsert.get(), 3, play.name);
                bindText(playInsert.get(), 4, play.playType);

                if (play.sourceCsvLine > 0)
                    bindInt(playInsert.get(), 5, play.sourceCsvLine);
                else
                    bindNull(playInsert.get(), 5);

                playInsert.stepDone(context.db);
                const sqlite3_int64 playId =
                    lastInsertId(context.db);
                context.inserted();

                for (const auto& [key, value] : play.adjustments)
                {
                    adjustmentInsert.reset();
                    bindInt64(adjustmentInsert.get(), 1, playId);
                    bindText(adjustmentInsert.get(), 2, key);
                    bindInt(adjustmentInsert.get(), 3, value);
                    adjustmentInsert.stepDone(context.db);
                    context.inserted();
                }

                for (const MatchupData& row : play.matchups)
                {
                    matchupInsert.reset();
                    bindInt64(matchupInsert.get(), 1, playId);
                    bindInt(matchupInsert.get(), 2, row.whiteD20);
                    bindText(
                        matchupInsert.get(),
                        3,
                        row.offensivePosition);
                    bindText(
                        matchupInsert.get(),
                        4,
                        row.offensiveSkill);
                    bindText(
                        matchupInsert.get(),
                        5,
                        row.defensivePosition);
                    bindText(
                        matchupInsert.get(),
                        6,
                        row.defensiveSkill);

                    if (row.sourceCsvLine > 0)
                        bindInt(matchupInsert.get(), 7, row.sourceCsvLine);
                    else
                        bindNull(matchupInsert.get(), 7);

                    matchupInsert.stepDone(context.db);
                    const sqlite3_int64 matchupId =
                        lastInsertId(context.db);
                    context.inserted();

                    for (int d10 = 0; d10 < 10; ++d10)
                    {
                        if (!row.hasOutcome[d10])
                        {
                            throw BuildException(
                                "Offensive matchup is missing OUTCOME_" +
                                std::to_string(d10) + ".",
                                file,
                                row.beginLine);
                        }

                        outcomeInsert.reset();
                        bindInt64(
                            outcomeInsert.get(),
                            1,
                            matchupId);
                        bindInt(outcomeInsert.get(), 2, d10);
                        bindText(
                            outcomeInsert.get(),
                            3,
                            row.outcomes[d10]);
                        outcomeInsert.stepDone(context.db);
                        context.inserted();
                    }

                    ++totalRows;
                }
            };

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "OFFENSIVE_PLAY_BEGIN")
            {
                play = PlayData{};
                play.beginLine = tag.line;
                inPlay = true;
                continue;
            }

            if (!inPlay)
            {
                continue;
            }

            if (tag.tag == "MATCHUP_BEGIN")
            {
                matchup = MatchupData{};
                matchup.beginLine = tag.line;
                inMatchup = true;
                continue;
            }

            if (tag.tag == "MATCHUP_END")
            {
                if (!inMatchup)
                {
                    throw BuildException(
                        "MATCHUP_END without MATCHUP_BEGIN.",
                        file,
                        tag.line);
                }

                play.matchups.push_back(std::move(matchup));
                inMatchup = false;
                continue;
            }

            if (tag.tag == "OFFENSIVE_PLAY_END")
            {
                if (inMatchup)
                {
                    throw BuildException(
                        "OFFENSIVE_PLAY_END encountered inside MATCHUP block.",
                        file,
                        tag.line);
                }

                insertPlay();
                inPlay = false;
                continue;
            }

            if (inMatchup)
            {
                if (tag.tag == "WHITE_D20")
                    matchup.whiteD20 = parseInt(tag.value, file, tag.line, "WHITE_D20");
                else if (tag.tag == "OFFENSIVE_POSITION")
                    matchup.offensivePosition = tag.value;
                else if (tag.tag == "OFFENSIVE_SKILL")
                    matchup.offensiveSkill = tag.value;
                else if (tag.tag == "DEFENSIVE_POSITION")
                    matchup.defensivePosition = tag.value;
                else if (tag.tag == "DEFENSIVE_SKILL")
                    matchup.defensiveSkill = tag.value;
                else if (tag.tag == "SOURCE_CSV_LINE")
                    matchup.sourceCsvLine =
                    parseInt(tag.value, file, tag.line, "SOURCE_CSV_LINE");
                else if (tag.tag.rfind("OUTCOME_", 0) == 0)
                {
                    const int d10 =
                        parseInt(
                            tag.tag.substr(8),
                            file,
                            tag.line,
                            "OUTCOME index");

                    if (d10 < 0 || d10 > 9)
                    {
                        throw BuildException(
                            "OUTCOME index must be 0..9.",
                            file,
                            tag.line);
                    }

                    matchup.outcomes[d10] = tag.value;
                    matchup.hasOutcome[d10] = true;
                }

                continue;
            }

            if (tag.tag == "PLAY_KEY") play.playKey = tag.value;
            else if (tag.tag == "NAME") play.name = tag.value;
            else if (tag.tag == "PLAY_TYPE") play.playType = tag.value;
            else if (tag.tag == "SOURCE_CSV_LINE")
                play.sourceCsvLine =
                parseInt(tag.value, file, tag.line, "SOURCE_CSV_LINE");
            else if (tag.tag.rfind("ADJUST_", 0) == 0)
                play.adjustments.emplace_back(
                    tag.tag.substr(7),
                    parseInt(
                        tag.value,
                        file,
                        tag.line,
                        tag.tag.c_str()));
        }

        return totalRows;
    }

    std::uint64_t loadDefensivePlays(BuildContext& context,
        const std::filesystem::path& file)
    {
        const auto tags = readTaggedFile(file);

        struct DefenseData
        {
            std::string key;
            std::string name;
            int sourceCsvLine = 0;
            int minMenOnLine = 0;
            std::string passRush;
            std::string passCoverage;
            int playActionModifier = 0;
            std::size_t beginLine = 0;
        };

        DefenseData current;
        bool inDefense = false;
        std::uint64_t count = 0;

        Statement insert(
            context.db,
            R"SQL(
INSERT INTO DefensivePlays
(
    RulesetID, DefenseKey, Name, SourceCsvLine,
    MinMenOnLine, PassRush, PassCoverage, PlayActionModifier
)
VALUES(?, ?, ?, ?, ?, ?, ?, ?);
)SQL");

        for (const TagLine& tag : tags)
        {
            if (tag.tag == "DEFENSIVE_PLAY_BEGIN")
            {
                current = DefenseData{};
                current.beginLine = tag.line;
                inDefense = true;
                continue;
            }

            if (!inDefense)
            {
                continue;
            }

            if (tag.tag == "DEFENSIVE_PLAY_END")
            {
                insert.reset();
                bindInt64(insert.get(), 1, context.rulesetId);
                bindText(insert.get(), 2, current.key);
                bindText(insert.get(), 3, current.name);

                if (current.sourceCsvLine > 0)
                    bindInt(insert.get(), 4, current.sourceCsvLine);
                else
                    bindNull(insert.get(), 4);

                bindInt(insert.get(), 5, current.minMenOnLine);
                bindText(insert.get(), 6, current.passRush);
                bindText(insert.get(), 7, current.passCoverage);
                bindInt(
                    insert.get(),
                    8,
                    current.playActionModifier);

                insert.stepDone(context.db);

                ++count;
                context.inserted();
                inDefense = false;
                continue;
            }

            if (tag.tag == "DEFENSE_KEY") current.key = tag.value;
            else if (tag.tag == "NAME") current.name = tag.value;
            else if (tag.tag == "SOURCE_CSV_LINE")
                current.sourceCsvLine =
                parseInt(tag.value, file, tag.line, "SOURCE_CSV_LINE");
            else if (tag.tag == "MIN_MEN_ON_LINE")
                current.minMenOnLine =
                parseInt(tag.value, file, tag.line, "MIN_MEN_ON_LINE");
            else if (tag.tag == "PASS_RUSH") current.passRush = tag.value;
            else if (tag.tag == "PASS_COVERAGE") current.passCoverage = tag.value;
            else if (tag.tag == "PLAY_ACTION_MODIFIER")
                current.playActionModifier =
                parseInt(tag.value, file, tag.line, "PLAY_ACTION_MODIFIER");
        }

        return count;
    }

    std::uint64_t tableCount(sqlite3* db, const char* table)
    {
        const std::string sql =
            "SELECT COUNT(*) FROM " + std::string(table) + ";";

        Statement statement(db, sql.c_str());

        if (!statement.stepRow(db))
        {
            throw BuildException(
                "COUNT query returned no row for table " +
                std::string(table));
        }

        return static_cast<std::uint64_t>(
            sqlite3_column_int64(statement.get(), 0));
    }

    void validateInitialCounts(BuildContext& context)
    {
        struct ExpectedCount
        {
            const char* table;
            std::uint64_t expected;
        };

        const ExpectedCount counts[] =
        {
            { "Teams", 28 },
            { "TeamSeasons", 28 },
            { "Players", 1008 },
            { "ScheduleGames", 233 },
            { "OffensivePlays", 36 },
            { "OffensivePlayMatchups", 720 },
            { "OffensivePlayOutcomes", 7200 },
            { "DefensivePlays", 12 }
        };

        for (const ExpectedCount& expected : counts)
        {
            const std::uint64_t actual =
                tableCount(context.db, expected.table);

            if (actual != expected.expected)
            {
                throw BuildException(
                    std::string("Initial package count validation failed for ") +
                    expected.table + ": expected " +
                    std::to_string(expected.expected) +
                    ", found " + std::to_string(actual) + ".");
            }
        }
    }

    void validateDatabase(sqlite3* db)
    {
        {
            Statement statement(db, "PRAGMA foreign_key_check;");

            if (statement.stepRow(db))
            {
                const char* tableName =
                    reinterpret_cast<const char*>(
                        sqlite3_column_text(statement.get(), 0));

                throw BuildException(
                    "PRAGMA foreign_key_check failed in table: " +
                    std::string(tableName != nullptr ? tableName : "<unknown>"));
            }
        }

        {
            Statement statement(db, "PRAGMA integrity_check;");

            if (!statement.stepRow(db))
            {
                throw BuildException(
                    "PRAGMA integrity_check returned no result.");
            }

            const char* value =
                reinterpret_cast<const char*>(
                    sqlite3_column_text(statement.get(), 0));

            if (value == nullptr || std::string(value) != "ok")
            {
                throw BuildException(
                    "PRAGMA integrity_check failed: " +
                    std::string(value != nullptr ? value : "<null>"));
            }
        }
    }

    void publishDatabase(const std::filesystem::path& temporary,
        const std::filesystem::path& finalPath,
        bool overwrite)
    {
        if (!std::filesystem::exists(finalPath))
        {
            std::filesystem::rename(temporary, finalPath);
            return;
        }

        if (!overwrite)
        {
            throw BuildException(
                "Output database already exists and overwriteExisting is false: " +
                pathUtf8(finalPath));
        }

        std::filesystem::path backup = finalPath;
        backup += ".previous";

        std::error_code ec;
        std::filesystem::remove(backup, ec);
        ec.clear();

        std::filesystem::rename(finalPath, backup, ec);
        if (ec)
        {
            throw BuildException(
                "Unable to move existing Core database aside: " +
                ec.message());
        }

        std::filesystem::rename(temporary, finalPath, ec);
        if (ec)
        {
            std::error_code restoreError;
            std::filesystem::rename(
                backup,
                finalPath,
                restoreError);

            throw BuildException(
                "Unable to publish newly built Core database: " +
                ec.message());
        }

        std::filesystem::remove(backup, ec);
    }
}

struct CoreDatabaseBuildAccess
{
    static sqlite3* handle(Database& database) noexcept
    {
        return database.db_;
    }

    static bool openForBuild(Database& database,
        const std::filesystem::path& path)
    {
        database.close();
        database.lastError_.clear();

        sqlite3* handle = nullptr;

        const int flags =
            SQLITE_OPEN_READWRITE |
            SQLITE_OPEN_CREATE |
            SQLITE_OPEN_NOMUTEX;

        const int rc =
            sqlite3_open_v2(
                pathUtf8(path).c_str(),
                &handle,
                flags,
                nullptr);

        if (rc != SQLITE_OK)
        {
            database.lastError_ =
                handle != nullptr
                ? sqlite3_errmsg(handle)
                : "Unable to create SQLite database.";

            if (handle != nullptr)
            {
                sqlite3_close_v2(handle);
            }

            return false;
        }

        database.db_ = handle;
        database.path_ = path;
        return true;
    }
};

// -----------------------------------------------------------------------------
// Database
// -----------------------------------------------------------------------------

Database::~Database()
{
    close();
}

Database::Database(Database&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)),
    path_(std::move(other.path_)),
    lastError_(std::move(other.lastError_))
{
}

Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other)
    {
        close();

        db_ = std::exchange(other.db_, nullptr);
        path_ = std::move(other.path_);
        lastError_ = std::move(other.lastError_);
    }

    return *this;
}

bool Database::open(const std::filesystem::path& databasePath,
    DatabaseOpenMode mode)
{
    close();
    lastError_.clear();

    int flags = SQLITE_OPEN_NOMUTEX;

    switch (mode)
    {
    case DatabaseOpenMode::ReadOnly:
        flags |= SQLITE_OPEN_READONLY;
        break;

    case DatabaseOpenMode::ReadWrite:
        flags |= SQLITE_OPEN_READWRITE;
        break;
    }

    sqlite3* handle = nullptr;

    const int rc =
        sqlite3_open_v2(
            pathUtf8(databasePath).c_str(),
            &handle,
            flags,
            nullptr);

    if (rc != SQLITE_OK)
    {
        lastError_ =
            handle != nullptr
            ? sqlite3_errmsg(handle)
            : "Unable to open SQLite database.";

        if (handle != nullptr)
        {
            sqlite3_close_v2(handle);
        }

        return false;
    }

    db_ = handle;
    path_ = databasePath;

    if (sqlite3_exec(
        db_,
        "PRAGMA foreign_keys = ON;",
        nullptr,
        nullptr,
        nullptr) != SQLITE_OK)
    {
        lastError_ = sqlite3_errmsg(db_);
        close();
        return false;
    }

    return true;
}

void Database::close() noexcept
{
    if (db_ != nullptr)
    {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }

    path_.clear();
}

bool Database::isOpen() const noexcept
{
    return db_ != nullptr;
}

const std::filesystem::path& Database::path() const noexcept
{
    return path_;
}

const std::string& Database::lastError() const noexcept
{
    return lastError_;
}

// -----------------------------------------------------------------------------
// Core database build
// -----------------------------------------------------------------------------

CoreDatabaseBuildResult createCoreDatabaseFromSource(
    const CoreDatabaseBuildRequest& request)
{
    CoreDatabaseBuildResult result;
    result.databasePath = request.outputDatabase;

    std::filesystem::path temporaryDatabase;

    try
    {
        if (request.sourceRoot.empty())
        {
            throw BuildException("sourceRoot is required.");
        }

        if (request.ruleSet.empty())
        {
            throw BuildException("ruleSet is required.");
        }

        if (request.season <= 0)
        {
            throw BuildException("season must be greater than zero.");
        }

        if (request.outputDatabase.empty())
        {
            throw BuildException("outputDatabase is required.");
        }

        if (!std::filesystem::is_directory(request.sourceRoot))
        {
            throw BuildException(
                "Source root does not exist or is not a directory: " +
                pathUtf8(request.sourceRoot));
        }

        const std::filesystem::path rulesetDirectory =
            request.sourceRoot /
            "RuleSets" /
            request.ruleSet;

        const std::filesystem::path seasonDirectory =
            request.sourceRoot /
            "Seasons" /
            std::to_string(request.season);

        if (!std::filesystem::is_directory(rulesetDirectory))
        {
            throw BuildException(
                "Ruleset source directory is missing: " +
                pathUtf8(rulesetDirectory));
        }

        if (!std::filesystem::is_directory(seasonDirectory))
        {
            throw BuildException(
                "Season source directory is missing: " +
                pathUtf8(seasonDirectory));
        }

        if (std::filesystem::exists(request.outputDatabase) &&
            !request.overwriteExisting)
        {
            throw BuildException(
                "Output database already exists and overwriteExisting is false: " +
                pathUtf8(request.outputDatabase));
        }

        const std::filesystem::path parent =
            request.outputDatabase.parent_path();

        if (!parent.empty())
        {
            std::filesystem::create_directories(parent);
        }

        temporaryDatabase = request.outputDatabase;
        temporaryDatabase += ".building";

        {
            std::error_code ec;
            std::filesystem::remove(temporaryDatabase, ec);
        }

        Database database;

        if (!CoreDatabaseBuildAccess::openForBuild(
            database,
            temporaryDatabase))
        {
            throw BuildException(
                "Unable to create temporary Core database: " +
                database.lastError());
        }

        sqlite3* db = CoreDatabaseBuildAccess::handle(database);

        exec(db, "PRAGMA foreign_keys = ON;");
        exec(db, "PRAGMA journal_mode = DELETE;");
        exec(db, "PRAGMA synchronous = NORMAL;");
        exec(db, "BEGIN IMMEDIATE TRANSACTION;");

        BuildContext context;
        context.db = db;
        context.result = &result;
        context.sourceRoot = request.sourceRoot;

        try
        {
            createSchema(db);

            insertMetadata(db, context, "CoreSchemaVersion", "1");
            insertMetadata(db, context, "Ruleset", request.ruleSet);
            insertMetadata(
                db,
                context,
                "Season",
                std::to_string(request.season));

            context.rulesetId =
                insertRuleset(context, request.ruleSet);

            const std::filesystem::path venues =
                requireFile(seasonDirectory, "venues.pffdata");

            const std::filesystem::path venueAliases =
                requireFile(
                    seasonDirectory,
                    "venue_aliases.pffdata");

            const std::filesystem::path league =
                requireFile(
                    seasonDirectory,
                    "league_structure.pffdata");

            const std::filesystem::path players =
                requireFile(
                    seasonDirectory,
                    "players.pffdata");

            const std::filesystem::path offensiveUnitRatings =
                requireFile(
                    seasonDirectory,
                    "basic_offensive_unit_ratings.pffdata");

            const std::filesystem::path defensiveUnitRatings =
                requireFile(
                    seasonDirectory,
                    "basic_defensive_unit_ratings.pffdata");

            const std::filesystem::path schedule =
                requireFile(
                    seasonDirectory,
                    "schedule.pffdata");

            const std::filesystem::path resultCodes =
                requireFile(
                    rulesetDirectory,
                    "result_codes.pffdata");

            const std::filesystem::path offensivePlays =
                requireFile(
                    rulesetDirectory,
                    "offensive_plays.pffdata");

            const std::filesystem::path defensivePlays =
                requireFile(
                    rulesetDirectory,
                    "defensive_plays.pffdata");

            auto loadAndRegister =
                [&](const std::filesystem::path& file,
                    const std::string& family,
                    auto&& loader)
                {
                    const std::uint64_t records =
                        loader(file);

                    registerSourceFile(
                        context,
                        file,
                        family,
                        "IMPORTED",
                        records);

                    ++result.filesProcessed;

                    context.diagnostic(
                        DatabaseDiagnosticSeverity::Info,
                        "Imported " + std::to_string(records) +
                        " record(s) from " +
                        pathUtf8(file.filename()) + ".",
                        file);
                };

            // Dependency order:
            // venues -> teams/team-seasons -> players/unit-ratings/schedule.
            loadAndRegister(
                venues,
                "VENUES",
                [&](const auto& file)
                {
                    return loadVenues(context, file);
                });

            loadAndRegister(
                venueAliases,
                "VENUE_ALIASES",
                [&](const auto& file)
                {
                    return loadVenueAliases(context, file);
                });

            loadAndRegister(
                league,
                "LEAGUE_STRUCTURE",
                [&](const auto& file)
                {
                    return loadLeagueStructure(
                        context,
                        file,
                        request.season);
                });

            loadAndRegister(
                players,
                "PLAYERS",
                [&](const auto& file)
                {
                    return loadPlayers(context, file);
                });

            loadAndRegister(
                offensiveUnitRatings,
                "BASIC_OFFENSIVE_UNIT_RATINGS",
                [&](const auto& file)
                {
                    return loadBasicOffensiveRatings(
                        context,
                        file);
                });

            loadAndRegister(
                defensiveUnitRatings,
                "BASIC_DEFENSIVE_UNIT_RATINGS",
                [&](const auto& file)
                {
                    return loadBasicDefensiveRatings(
                        context,
                        file);
                });

            loadAndRegister(
                schedule,
                "SCHEDULE",
                [&](const auto& file)
                {
                    return loadSchedule(
                        context,
                        file,
                        request.season);
                });

            // Independent First Edition ruleset foundation.
            loadAndRegister(
                resultCodes,
                "RESULT_CODES",
                [&](const auto& file)
                {
                    return loadResultCodes(context, file);
                });

            loadAndRegister(
                offensivePlays,
                "OFFENSIVE_PLAYS",
                [&](const auto& file)
                {
                    return loadOffensivePlays(context, file);
                });

            loadAndRegister(
                defensivePlays,
                "DEFENSIVE_PLAYS",
                [&](const auto& file)
                {
                    return loadDefensivePlays(context, file);
                });

            // Discover all remaining source files. They are recorded and
            // explicitly reported rather than silently skipped.
            const std::unordered_map<std::string, bool> supportedRulesetFiles =
            {
                { "result_codes.pffdata", true },
                { "offensive_plays.pffdata", true },
                { "defensive_plays.pffdata", true }
            };

            const std::unordered_map<std::string, bool> supportedSeasonFiles =
            {
                { "venues.pffdata", true },
                { "venue_aliases.pffdata", true },
                { "league_structure.pffdata", true },
                { "players.pffdata", true },
                { "basic_offensive_unit_ratings.pffdata", true },
                { "basic_defensive_unit_ratings.pffdata", true },
                { "schedule.pffdata", true }
            };

            auto discoverUnsupported =
                [&](const std::filesystem::path& directory,
                    const auto& supported)
                {
                    for (const auto& entry :
                        std::filesystem::directory_iterator(directory))
                    {
                        if (!entry.is_regular_file() ||
                            entry.path().extension() != ".pffdata")
                        {
                            continue;
                        }

                        const std::string fileName =
                            entry.path().filename().string();

                        if (supported.find(fileName) != supported.end())
                        {
                            continue;
                        }

                        registerSourceFile(
                            context,
                            entry.path(),
                            "UNSUPPORTED_YET",
                            "DISCOVERED_NOT_IMPORTED",
                            0);

                        context.diagnostic(
                            DatabaseDiagnosticSeverity::Warning,
                            "Source file discovered but not imported by the initial Core builder: " +
                            fileName,
                            entry.path());
                    }
                };

            discoverUnsupported(
                rulesetDirectory,
                supportedRulesetFiles);

            discoverUnsupported(
                seasonDirectory,
                supportedSeasonFiles);

            validateInitialCounts(context);
            validateDatabase(db);

            exec(db, "COMMIT;");

            database.close();

            publishDatabase(
                temporaryDatabase,
                request.outputDatabase,
                request.overwriteExisting);

            result.success = true;
            result.databasePath = request.outputDatabase;

            result.diagnostics.push_back(
                {
                    DatabaseDiagnosticSeverity::Info,
                    "Core database build completed successfully.",
                    request.outputDatabase,
                    0
                });
        }
        catch (...)
        {
            try
            {
                exec(db, "ROLLBACK;");
            }
            catch (...)
            {
                // Preserve the original exception.
            }

            database.close();
            throw;
        }
    }
    catch (const BuildException& ex)
    {
        DatabaseDiagnostic diagnostic;
        diagnostic.severity = DatabaseDiagnosticSeverity::Error;
        diagnostic.message = ex.what();
        diagnostic.sourceFile = ex.sourceFile();
        diagnostic.sourceLine = ex.sourceLine();

        result.diagnostics.push_back(std::move(diagnostic));

        if (!temporaryDatabase.empty())
        {
            std::error_code ec;
            std::filesystem::remove(temporaryDatabase, ec);
        }

        result.success = false;
    }
    catch (const std::exception& ex)
    {
        result.diagnostics.push_back(
            {
                DatabaseDiagnosticSeverity::Error,
                ex.what(),
                {},
                0
            });

        if (!temporaryDatabase.empty())
        {
            std::error_code ec;
            std::filesystem::remove(temporaryDatabase, ec);
        }

        result.success = false;
    }

    return result;
}

