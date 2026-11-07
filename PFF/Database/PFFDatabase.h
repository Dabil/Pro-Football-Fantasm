#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Forward declaration keeps sqlite3.h out of the public API header.
struct sqlite3;

struct CorePersonNicknameSource
{
    std::int64_t sourceFileId = 0;
    std::size_t sourceLine = 0;
    std::string relativePath;
};
struct CorePersonNickname
{
    std::string nickname, normalizedNickname;
    std::int64_t displayOrder = 0;
    std::vector<CorePersonNicknameSource> sources;
};
struct CorePersonNicknamesResult
{
    bool success = false;
    std::string errorMessage, normalizationPolicy, unicodeVersion;
    std::vector<CorePersonNickname> nicknames;
};
CorePersonNicknamesResult loadCorePersonNicknames(const std::filesystem::path& core,
                                                 const std::string& corePersonKey);

// -----------------------------------------------------------------------------
// Database connection
// -----------------------------------------------------------------------------

enum class DatabaseOpenMode
{
    ReadOnly,
    ReadWrite
};

class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Opens an existing SQLite database.
    // Core databases should normally be opened ReadOnly at runtime.
    bool open(const std::filesystem::path& databasePath,
        DatabaseOpenMode mode = DatabaseOpenMode::ReadOnly);

    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    sqlite3* db_ = nullptr;
    std::filesystem::path path_;
    std::string lastError_;

    // The Core builder will need direct SQLite access while constructing
    // the disposable compiled database. Runtime callers should not.
    friend struct CoreDatabaseBuildAccess;
    friend struct CoreDatabaseQueryAccess;

    // Transitional access for the generic GCE World database layer.
    // Database itself will eventually move into a GCE-owned header.
    friend struct WorldDatabaseAccess;
    friend struct WorldEditingAccess;
};

// -----------------------------------------------------------------------------
// Core database build API
// -----------------------------------------------------------------------------

// Explicit authored build configuration. No default quarter length or inferred zone.
struct CoreClockProfileInput
{
    std::string key, overtimePolicyKey, timingPolicyKey, policyContentHash;
    std::int32_t version = 1, quarterSeconds = 0, timingPolicyVersion = 1;
};
struct CoreTeamHomeZoneInput
{
    std::string teamSeasonKey, ianaZoneId, zoneDatabaseVersion;
    std::filesystem::path sourceFile;
    std::size_t sourceLine = 0;
};
struct CoreDatabaseBuildRequest
{
    // Canonical source root, normally:
    //
    //   Data/Source
    //
    // The builder resolves ruleset- and season-specific content beneath it.
    std::filesystem::path sourceRoot;

    // Initial PFF Classic values:
    //
    //   ruleSet = "FirstEdition"
    //   season  = 1989
    //
    // These are inputs rather than hardcoded assumptions so the same API
    // can build later PFF/GCE packages.
    std::string ruleSet;
    std::int32_t season = 0;

    // Final compiled Core database path.
    std::filesystem::path outputDatabase;

    // False by default so an existing Core is never destroyed accidentally.
    bool overwriteExisting = false;

    // Manifest mode is opt-in; legacy callers retain their existing contract.
    std::filesystem::path rulesetManifest;
    std::filesystem::path seasonManifest;
    bool validateOnly = false;
    std::vector<CoreClockProfileInput> clockProfiles;
    // Source evidence is mandatory; venue UTC offsets never imply home-city zones.
    std::vector<CoreTeamHomeZoneInput> teamHomeTimeZones;
};

enum class DatabaseDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

struct DatabaseDiagnostic
{
    DatabaseDiagnosticSeverity severity = DatabaseDiagnosticSeverity::Error;

    std::string message;

    // Optional source location. Empty/zero when the error is not tied to a
    // specific authored source record.
    std::filesystem::path sourceFile;
    std::size_t sourceLine = 0;
};

struct CoreDatabaseBuildResult
{
    bool success = false;

    std::filesystem::path databasePath;

    std::size_t filesProcessed = 0;
    std::uint64_t recordsInserted = 0;

    std::vector<DatabaseDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Builds a disposable/reproducible Core database from canonical editable
// source. The implementation will:
//
//   1. discover the selected ruleset + season .pffdata sources;
//   2. create the initial Core schema;
//   3. parse and validate supported source records;
//   4. populate the Core database transactionally;
//   5. run integrity/foreign-key validation;
//   6. publish the completed database only after a successful build.
//
// The first implementation can support only the initial source families we
// are loading now. Unsupported source families should produce an explicit
// diagnostic rather than being silently ignored.
CoreDatabaseBuildResult createCoreDatabaseFromSource(
    const CoreDatabaseBuildRequest& request);

// -----------------------------------------------------------------------------
// Core database read/query API
// -----------------------------------------------------------------------------

struct CoreTeamRecord
{
    std::int64_t teamId = 0;
    std::int64_t teamSeasonId = 0;

    std::string teamKey;
    std::string teamSeasonKey;
    std::string city;
    std::string nickname;
    std::string conference;
    std::string division;

    [[nodiscard]] std::string displayName() const
    {
        if (city.empty())
        {
            return nickname;
        }

        if (nickname.empty())
        {
            return city;
        }

        return city + " " + nickname;
    }
};

struct CoreTeamsLoadResult
{
    bool success = false;
    std::vector<CoreTeamRecord> teams;
    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Reads the team list for one season from an existing compiled Core database.
// This is intentionally a narrow first read API used by TeamsAndStandingsScreen.
// Broader repositories/query services can replace or build on this later.
CoreTeamsLoadResult loadCoreTeams(
    const std::filesystem::path& databasePath,
    std::int32_t season = 1989);


struct CoreScheduleGameRecord
{
    std::string sourceGameId;
    std::int32_t weekNumber = 0;

    std::string gameDate;
    std::string dayLabel;
    std::string timeLabel;
    std::string site;

    std::int64_t awayTeamSeasonId = 0;
    std::int64_t homeTeamSeasonId = 0;

    std::string awayCity;
    std::string awayNickname;
    std::string homeCity;
    std::string homeNickname;

    [[nodiscard]] std::string awayDisplayName() const
    {
        if (awayCity.empty())
        {
            return awayNickname;
        }

        if (awayNickname.empty())
        {
            return awayCity;
        }

        return awayCity + " " + awayNickname;
    }

    [[nodiscard]] std::string homeDisplayName() const
    {
        if (homeCity.empty())
        {
            return homeNickname;
        }

        if (homeNickname.empty())
        {
            return homeCity;
        }

        return homeCity + " " + homeNickname;
    }

    [[nodiscard]] std::string matchupDisplayName() const
    {
        return awayDisplayName() + " at " + homeDisplayName();
    }
};

struct CoreScheduleLoadResult
{
    bool success = false;

    std::vector<std::int32_t> weeks;
    std::vector<CoreScheduleGameRecord> games;
    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Reads the historical regular-season schedule for one season from an
// existing compiled Core database. Results are ordered by week/date/time.
CoreScheduleLoadResult loadCoreRegularSeasonSchedule(
    const std::filesystem::path& databasePath,
    std::int32_t season = 1989);



