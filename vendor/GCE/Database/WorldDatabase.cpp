#include "GCE/Database/WorldStorageHash.h"
#include "GCE/Database/WorldDatabase.h"

#include <sqlite3.h>
#include "GCE/Database/WorldTeamDataSchema.h"
#include "GCE/Database/WorldEventReadiness.h"
#include "GCE/Database/TeamDataSql.h"
#include <map>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Transitional shared Database access
// -----------------------------------------------------------------------------
//
// Database currently lives in PFFDatabase.h. WorldDatabase is GCE-owned, so it
// uses one narrow friend access point until Database itself moves into a generic
// GCE database header. This avoids introducing a second SQLite connection class.
// -----------------------------------------------------------------------------

struct WorldDatabaseAccess
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

        const std::string pathText = path.u8string();

        const int rc =
            sqlite3_open_v2(
                pathText.c_str(),
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

namespace
{
    class WorldDatabaseException final : public std::runtime_error
    {
    public:
        explicit WorldDatabaseException(std::string message)
            : std::runtime_error(std::move(message))
        {
        }
    };

    class Statement
    {
    public:
        Statement(sqlite3* db, const char* sql)
        {
            const int rc = sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr);
            if (rc != SQLITE_OK)
            {
                throw WorldDatabaseException(
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
                throw WorldDatabaseException(
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

            throw WorldDatabaseException(
                "SQLite query failed: " + std::string(sqlite3_errmsg(db)));
        }

    private:
        sqlite3_stmt* statement_ = nullptr;
    };

    std::string pathUtf8(const std::filesystem::path& path)
    {
        return path.u8string();
    }

    std::string columnText(sqlite3_stmt* statement, int column)
    {
        const unsigned char* value =
            sqlite3_column_text(statement, column);

        return value != nullptr
            ? reinterpret_cast<const char*>(value)
            : std::string{};
    }

    std::optional<std::int64_t> columnOptionalInt64(
        sqlite3_stmt* statement,
        int column)
    {
        if (sqlite3_column_type(statement, column) == SQLITE_NULL)
        {
            return std::nullopt;
        }

        return static_cast<std::int64_t>(
            sqlite3_column_int64(statement, column));
    }

    void bindText(sqlite3_stmt* statement, int index, const std::string& value)
    {
        if (sqlite3_bind_text(
            statement,
            index,
            value.c_str(),
            static_cast<int>(value.size()),
            SQLITE_TRANSIENT) != SQLITE_OK)
        {
            throw WorldDatabaseException("SQLite text bind failed.");
        }
    }

    void bindInt(sqlite3_stmt* statement, int index, std::int32_t value)
    {
        if (sqlite3_bind_int(statement, index, value) != SQLITE_OK)
        {
            throw WorldDatabaseException("SQLite integer bind failed.");
        }
    }

    void bindInt64(sqlite3_stmt* statement, int index, std::int64_t value)
    {
        if (sqlite3_bind_int64(
            statement,
            index,
            static_cast<sqlite3_int64>(value)) != SQLITE_OK)
        {
            throw WorldDatabaseException("SQLite 64-bit integer bind failed.");
        }
    }

    void bindDouble(sqlite3_stmt* statement, int index, double value)
    {
        if (sqlite3_bind_double(statement, index, value) != SQLITE_OK)
        {
            throw WorldDatabaseException("SQLite floating-point bind failed.");
        }
    }

    void bindNull(sqlite3_stmt* statement, int index)
    {
        if (sqlite3_bind_null(statement, index) != SQLITE_OK)
        {
            throw WorldDatabaseException("SQLite null bind failed.");
        }
    }

    void bindOptionalText(
        sqlite3_stmt* statement,
        int index,
        const std::string& value)
    {
        if (value.empty())
        {
            bindNull(statement, index);
        }
        else
        {
            bindText(statement, index, value);
        }
    }

    void bindOptionalInt64(
        sqlite3_stmt* statement,
        int index,
        const std::optional<std::int64_t>& value)
    {
        if (value.has_value())
        {
            bindInt64(statement, index, *value);
        }
        else
        {
            bindNull(statement, index);
        }
    }

    void exec(sqlite3* db, const char* sql)
    {
        char* error = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);

        if (rc != SQLITE_OK)
        {
            std::string message =
                error != nullptr
                ? error
                : sqlite3_errmsg(db);

            sqlite3_free(error);
            throw WorldDatabaseException("SQLite exec failed: " + message);
        }
    }

    std::int64_t lastInsertId(sqlite3* db)
    {
        return static_cast<std::int64_t>(sqlite3_last_insert_rowid(db));
    }

    std::int64_t scalarInt64(sqlite3* db, const char* sql)
    {
        Statement statement(db, sql);
        if (!statement.stepRow(db))
        {
            throw WorldDatabaseException("Expected scalar query to return one row.");
        }

        return static_cast<std::int64_t>(
            sqlite3_column_int64(statement.get(), 0));
    }

    std::string readCoreMetadata(sqlite3* db, const std::string& key)
    {
        Statement statement(
            db,
            "SELECT MetadataValue FROM CoreMetadata WHERE MetadataKey = ?;");

        bindText(statement.get(), 1, key);

        if (!statement.stepRow(db))
        {
            return {};
        }

        return columnText(statement.get(), 0);
    }

    std::int32_t parseRequiredInt(
        const std::string& value,
        const char* fieldName)
    {
        std::int32_t result = 0;
        const char* begin = value.data();
        const char* end = value.data() + value.size();
        const auto parsed = std::from_chars(begin, end, result);

        if (parsed.ec != std::errc{} || parsed.ptr != end)
        {
            throw WorldDatabaseException(
                std::string("Invalid integer Core metadata for ") +
                fieldName + ": '" + value + "'.");
        }

        return result;
    }

    void validateSqliteDatabase(sqlite3* db, const std::string& label)
    {
        {
            Statement foreignKeys(db, "PRAGMA foreign_key_check;");
            if (foreignKeys.stepRow(db))
            {
                throw WorldDatabaseException(
                    label + " failed PRAGMA foreign_key_check.");
            }
        }

        {
            Statement integrity(db, "PRAGMA integrity_check;");
            if (!integrity.stepRow(db))
            {
                throw WorldDatabaseException(
                    label + " did not return an integrity-check result.");
            }

            const std::string value = columnText(integrity.get(), 0);
            if (value != "ok")
            {
                throw WorldDatabaseException(
                    label + " failed PRAGMA integrity_check: " + value);
            }
        }
    }

    // -------------------------------------------------------------------------
    // SHA-256
    // -------------------------------------------------------------------------

    using WorldStorageHash::Sha256;

    std::string sha256File(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            throw WorldDatabaseException(
                "Unable to open file for SHA-256: " + pathUtf8(path));
        }

        Sha256 sha;
        std::array<char, 64 * 1024> buffer{};

        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = input.gcount();

            if (count > 0)
            {
                sha.update(
                    reinterpret_cast<const std::uint8_t*>(buffer.data()),
                    static_cast<std::size_t>(count));
            }
        }

        if (!input.eof())
        {
            throw WorldDatabaseException(
                "Failed while reading file for SHA-256: " + pathUtf8(path));
        }

        return sha.finish();
    }

    // -------------------------------------------------------------------------
    // Calendar / schedule-clock normalization
    // -------------------------------------------------------------------------

    struct LocalDateTime
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
    };

    bool isLeapYear(int year)
    {
        return (year % 4 == 0 && year % 100 != 0) ||
            (year % 400 == 0);
    }

    int daysInMonth(int year, int month)
    {
        static constexpr int Days[] =
        {
            0, 31, 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31
        };

        if (month < 1 || month > 12)
        {
            return 0;
        }

        if (month == 2 && isLeapYear(year))
        {
            return 29;
        }

        return Days[month];
    }

    std::int64_t civilDays(int year, int month, int day)
    {
        year -= month <= 2;
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const int mp = month + (month > 2 ? -3 : 9);
        const unsigned doy =
            static_cast<unsigned>((153 * mp + 2) / 5 + day - 1);
        const unsigned doe =
            yoe * 365U + yoe / 4U - yoe / 100U + doy;

        return static_cast<std::int64_t>(era) * 146097 +
            static_cast<std::int64_t>(doe) - 719468;
    }

    int weekdaySundayZero(int year, int month, int day)
    {
        std::int64_t value = (civilDays(year, month, day) + 4) % 7;
        if (value < 0)
        {
            value += 7;
        }
        return static_cast<int>(value);
    }

    int firstSunday(int year, int month)
    {
        const int weekday = weekdaySundayZero(year, month, 1);
        return 1 + ((7 - weekday) % 7);
    }

    int nthSunday(int year, int month, int n)
    {
        return firstSunday(year, month) + (n - 1) * 7;
    }

    int lastSunday(int year, int month)
    {
        const int lastDay = daysInMonth(year, month);
        return lastDay - weekdaySundayZero(year, month, lastDay);
    }

    bool sameDate(const LocalDateTime& value, int month, int day)
    {
        return value.month == month && value.day == day;
    }

    bool dateAfter(const LocalDateTime& value, int month, int day)
    {
        return value.month > month ||
            (value.month == month && value.day > day);
    }

    bool dateBefore(const LocalDateTime& value, int month, int day)
    {
        return value.month < month ||
            (value.month == month && value.day < day);
    }

    bool isAmericaNewYorkDst(const LocalDateTime& value)
    {
        int startMonth = 0;
        int startDay = 0;
        int endMonth = 10;
        int endDay = lastSunday(value.year, 10);

        if (value.year >= 2007)
        {
            startMonth = 3;
            startDay = nthSunday(value.year, 3, 2);
            endMonth = 11;
            endDay = firstSunday(value.year, 11);
        }
        else if (value.year >= 1987)
        {
            startMonth = 4;
            startDay = firstSunday(value.year, 4);
        }
        else if (value.year == 1974)
        {
            startMonth = 1;
            startDay = 6;
        }
        else if (value.year == 1975)
        {
            startMonth = 2;
            startDay = 23;
        }
        else if (value.year >= 1966)
        {
            startMonth = 4;
            startDay = lastSunday(value.year, 4);
        }
        else
        {
            throw WorldDatabaseException(
                "America/New_York schedule conversion before 1966 is not "
                "implemented by WorldDB v1.");
        }

        if ((sameDate(value,startMonth,startDay) && value.hour==2) ||
            (sameDate(value,endMonth,endDay) && value.hour==1))
            throw WorldDatabaseException("AMBIGUOUS_LOCAL_TIME: schedule kickoff is in a nonexistent or repeated local hour; an explicit disambiguation policy is required.");
        if (dateBefore(value, startMonth, startDay) ||
            dateAfter(value, endMonth, endDay))
        {
            return false;
        }

        if (dateAfter(value, startMonth, startDay) &&
            dateBefore(value, endMonth, endDay))
        {
            return true;
        }

        if (sameDate(value, startMonth, startDay))
        {
            // U.S. spring transition occurs at 02:00 local standard time.
            return value.hour >= 2;
        }

        if (sameDate(value, endMonth, endDay))
        {
            // U.S. fall transition occurs at 02:00 local daylight time.
            // NFL schedule data does not use the ambiguous repeated 01:xx hour.
            return value.hour < 2;
        }

        return false;
    }

    LocalDateTime parseLocalDateTime(
        const std::string& dateText,
        const std::string& timeText)
    {
        LocalDateTime result;

        if (dateText.size() != 10 ||
            dateText[4] != '-' ||
            dateText[7] != '-')
        {
            throw WorldDatabaseException(
                "Schedule date must use YYYY-MM-DD: '" + dateText + "'.");
        }

        auto parsePart = [&](std::size_t start, std::size_t count, const char* name)
            {
                int value = 0;
                const char* begin = dateText.data() + start;
                const char* end = begin + count;
                const auto parsed = std::from_chars(begin, end, value);

                if (parsed.ec != std::errc{} || parsed.ptr != end)
                {
                    throw WorldDatabaseException(
                        std::string("Invalid schedule ") + name +
                        " in date '" + dateText + "'.");
                }

                return value;
            };

        result.year = parsePart(0, 4, "year");
        result.month = parsePart(5, 2, "month");
        result.day = parsePart(8, 2, "day");

        if (result.month < 1 || result.month > 12 ||
            result.day < 1 ||
            result.day > daysInMonth(result.year, result.month))
        {
            throw WorldDatabaseException(
                "Invalid schedule date: '" + dateText + "'.");
        }

        std::string compact;
        compact.reserve(timeText.size());
        for (char c : timeText)
        {
            if (!std::isspace(static_cast<unsigned char>(c)))
            {
                compact.push_back(
                    static_cast<char>(
                        std::toupper(static_cast<unsigned char>(c))));
            }
        }

        if (compact.size() < 6)
        {
            throw WorldDatabaseException(
                "Unsupported schedule time: '" + timeText + "'.");
        }

        const bool isAm = compact.size() >= 2 &&
            compact.substr(compact.size() - 2) == "AM";
        const bool isPm = compact.size() >= 2 &&
            compact.substr(compact.size() - 2) == "PM";

        if (!isAm && !isPm)
        {
            throw WorldDatabaseException(
                "Schedule time must include AM or PM: '" + timeText + "'.");
        }

        compact.resize(compact.size() - 2);

        const std::size_t colon = compact.find(':');
        if (colon == std::string::npos)
        {
            throw WorldDatabaseException(
                "Schedule time must use H:MMAM/H:MMPM: '" + timeText + "'.");
        }

        int hour12 = 0;
        int minute = 0;

        const auto hourParsed = std::from_chars(
            compact.data(),
            compact.data() + colon,
            hour12);

        const auto minuteParsed = std::from_chars(
            compact.data() + colon + 1,
            compact.data() + compact.size(),
            minute);

        if (hourParsed.ec != std::errc{} ||
            hourParsed.ptr != compact.data() + colon ||
            minuteParsed.ec != std::errc{} ||
            minuteParsed.ptr != compact.data() + compact.size() ||
            hour12 < 1 || hour12 > 12 || minute < 0 || minute > 59)
        {
            throw WorldDatabaseException(
                "Invalid schedule time: '" + timeText + "'.");
        }

        result.hour = hour12 % 12;
        if (isPm)
        {
            result.hour += 12;
        }
        result.minute = minute;
        result.second = 0;

        return result;
    }

    int parseFixedUtcOffsetMinutes(const std::string& zoneId)
    {
        // Optional generic fixed-offset support: UTC+05:30 / UTC-04:00.
        if (zoneId.size() != 9 || zoneId.rfind("UTC", 0) != 0 ||
            (zoneId[3] != '+' && zoneId[3] != '-') || zoneId[6] != ':')
        {
            throw WorldDatabaseException(
                "Unsupported ScheduleClockZoneId '" + zoneId +
                "'. WorldDB v1 currently supports UTC, fixed UTC+/-HH:MM "
                "zones, and America/New_York.");
        }

        int hours = 0;
        int minutes = 0;

        const auto hourParsed = std::from_chars(
            zoneId.data() + 4,
            zoneId.data() + 6,
            hours);

        const auto minuteParsed = std::from_chars(
            zoneId.data() + 7,
            zoneId.data() + 9,
            minutes);

        if (hourParsed.ec != std::errc{} || minuteParsed.ec != std::errc{} ||
            hours > 23 || minutes > 59)
        {
            throw WorldDatabaseException(
                "Invalid fixed ScheduleClockZoneId '" + zoneId + "'.");
        }

        int total = hours * 60 + minutes;
        if (zoneId[3] == '-')
        {
            total = -total;
        }
        return total;
    }

    int utcOffsetMinutes(
        const std::string& zoneId,
        const LocalDateTime& local)
    {
        if (zoneId == "UTC" || zoneId == "Etc/UTC" || zoneId == "GMT")
        {
            return 0;
        }

        if (zoneId == "America/New_York")
        {
            return isAmericaNewYorkDst(local) ? -4 * 60 : -5 * 60;
        }

        if (zoneId.rfind("UTC", 0) == 0)
        {
            return parseFixedUtcOffsetMinutes(zoneId);
        }

        throw WorldDatabaseException(
            "Unsupported ScheduleClockZoneId '" + zoneId +
            "'. Add support to the generic GCE schedule-clock normalizer "
            "before creating this World.");
    }

    std::string formatUtc(std::int64_t epochSeconds)
    {
        std::int64_t days = epochSeconds / 86400;
        std::int64_t seconds = epochSeconds % 86400;

        if (seconds < 0)
        {
            seconds += 86400;
            --days;
        }

        // Howard Hinnant civil_from_days algorithm.
        std::int64_t z = days + 719468;
        const std::int64_t era =
            (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe =
            static_cast<unsigned>(z - era * 146097);
        const unsigned yoe =
            (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int year = static_cast<int>(yoe) + static_cast<int>(era * 400);
        const unsigned doy =
            doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned day = doy - (153 * mp + 2) / 5 + 1;
        const unsigned month = mp + (mp < 10 ? 3 : -9);
        year += month <= 2;

        const int hour = static_cast<int>(seconds / 3600);
        const int minute = static_cast<int>((seconds % 3600) / 60);
        const int second = static_cast<int>(seconds % 60);

        char buffer[32]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%04d-%02u-%02uT%02d:%02d:%02dZ",
            year,
            month,
            day,
            hour,
            minute,
            second);

        return buffer;
    }

    std::string localToUtc(
        const std::string& zoneId,
        const std::string& dateText,
        const std::string& timeText)
    {
        const LocalDateTime local =
            parseLocalDateTime(dateText, timeText);

        const int offsetMinutes = utcOffsetMinutes(zoneId, local);

        const std::int64_t localEpoch =
            civilDays(local.year, local.month, local.day) * 86400 +
            static_cast<std::int64_t>(local.hour) * 3600 +
            static_cast<std::int64_t>(local.minute) * 60 +
            local.second;

        const std::int64_t utcEpoch =
            localEpoch - static_cast<std::int64_t>(offsetMinutes) * 60;

        return formatUtc(utcEpoch);
    }

    std::string nowUtc()
    {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        return formatUtc(static_cast<std::int64_t>(seconds));
    }

    std::uint64_t fnv1a64(const std::string& value)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (unsigned char c : value)
        {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    std::string hex64(std::uint64_t value)
    {
        static constexpr char Hex[] = "0123456789abcdef";
        std::string result(16, '0');
        for (int i = 15; i >= 0; --i)
        {
            result[static_cast<std::size_t>(i)] = Hex[value & 0x0fU];
            value >>= 4;
        }
        return result;
    }

    std::string generateWorldKey(
        const WorldDatabaseCreateRequest& request,
        const std::string& createdUtc)
    {
        const auto ticks =
            std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();

        const std::string material =
            request.worldName + "|" +
            pathUtf8(request.outputDatabase) + "|" +
            createdUtc + "|" +
            std::to_string(ticks);

        return "WORLD_" + hex64(fnv1a64(material));
    }

    std::optional<double> parseOptionalDouble(const std::string& value)
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        char* end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);

        // Venue coordinates are optional convenience data. If a historical
        // package preserves non-numeric coordinate text, leave the normalized
        // World coordinate NULL rather than blocking World creation.
        if (end == value.c_str() || *end != '\0')
        {
            return std::nullopt;
        }

        return parsed;
    }

    // -------------------------------------------------------------------------
    // Schema
    // -------------------------------------------------------------------------

    void createWorldSchema(sqlite3* db)
    {
        exec(db, R"SQL(
CREATE TABLE WorldSchemaInfo
(
    SchemaInfoID          INTEGER PRIMARY KEY CHECK (SchemaInfoID = 1),
    SchemaVersion         INTEGER NOT NULL,
    SchemaFamily          TEXT NOT NULL,
    CreatedByEngineBuild  TEXT,
    LastMigratedByBuild   TEXT,
    CreatedUtc            TEXT NOT NULL,
    LastMigratedUtc       TEXT
);

CREATE TABLE Worlds
(
    WorldID          INTEGER PRIMARY KEY CHECK (WorldID = 1),
    WorldKey         TEXT NOT NULL UNIQUE,
    WorldName        TEXT NOT NULL,
    CreatedUtc       TEXT NOT NULL,
    WorldStatus      TEXT NOT NULL,
    CurrentTimeUtc   TEXT NOT NULL
);

CREATE TABLE WorldSettings
(
    WorldID       INTEGER NOT NULL,
    SettingKey    TEXT NOT NULL,
    SettingValue  TEXT NOT NULL,
    PRIMARY KEY (WorldID, SettingKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE WorldCoreSnapshots
(
    WorldCoreSnapshotID        INTEGER PRIMARY KEY,
    WorldID                    INTEGER NOT NULL,
    CorePackageKey             TEXT,
    CoreContentHash            TEXT,
    SourceManifestHash         TEXT,
    SimulationRulesProfileKey  TEXT NOT NULL,
    SeasonPackageKey           TEXT,
    SnapshotPath               TEXT NOT NULL,
    SnapshotHashAlgorithm      TEXT NOT NULL,
    SnapshotFileHash           TEXT NOT NULL,
    AddedUtc                   TEXT NOT NULL,
    UNIQUE (WorldID, SnapshotPath),
    UNIQUE (WorldID, SnapshotHashAlgorithm, SnapshotFileHash),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE WorldCompetitions
(
    WorldCompetitionID  INTEGER PRIMARY KEY,
    WorldID              INTEGER NOT NULL,
    CompetitionKey       TEXT NOT NULL,
    Name                 TEXT NOT NULL,
    CompetitionType      TEXT NOT NULL,
    LevelCode             TEXT,
    Active                INTEGER NOT NULL DEFAULT 1 CHECK (Active IN (0, 1)),
    UNIQUE (WorldID, CompetitionKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE CompetitionSeasons
(
    CompetitionSeasonID          INTEGER PRIMARY KEY,
    WorldCompetitionID           INTEGER NOT NULL,
    WorldCoreSnapshotID          INTEGER,
    SeasonKey                    TEXT NOT NULL,
    SeasonLabel                  TEXT NOT NULL,
    SeasonYear                   INTEGER NOT NULL,
    StartDateUtc                 TEXT,
    EndDateUtc                   TEXT,
    Status                       TEXT NOT NULL,
    CurrentPhaseCode             TEXT,
    SimulationRulesProfileKey    TEXT NOT NULL,
    GameRulesProfileKey          TEXT,
    CompetitionPolicyProfileKey  TEXT,
    DefaultSimulationProviderKey TEXT,
    ScheduleClockZoneId          TEXT,
    UNIQUE (WorldCompetitionID, SeasonKey),
    FOREIGN KEY (WorldCompetitionID)
        REFERENCES WorldCompetitions(WorldCompetitionID),
    FOREIGN KEY (WorldCoreSnapshotID)
        REFERENCES WorldCoreSnapshots(WorldCoreSnapshotID)
);

CREATE TABLE WorldCompetitionGroups
(
    WorldGroupID        INTEGER PRIMARY KEY,
    CompetitionSeasonID INTEGER NOT NULL,
    ParentWorldGroupID  INTEGER,
    HierarchyKey        TEXT NOT NULL DEFAULT 'PRIMARY',
    GroupKey            TEXT NOT NULL,
    GroupType           TEXT NOT NULL,
    GroupName           TEXT NOT NULL,
    SortOrder           INTEGER,
    UNIQUE (CompetitionSeasonID, GroupKey),
    FOREIGN KEY (CompetitionSeasonID)
        REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (ParentWorldGroupID)
        REFERENCES WorldCompetitionGroups(WorldGroupID)
);

CREATE TABLE WorldOrganizations
(
    WorldOrganizationID  INTEGER PRIMARY KEY,
    WorldID               INTEGER NOT NULL,
    OrganizationKey       TEXT NOT NULL,
    OrganizationType      TEXT NOT NULL,
    CanonicalName         TEXT NOT NULL,
    OriginCoreKey         TEXT,
    Active                INTEGER NOT NULL DEFAULT 1 CHECK (Active IN (0, 1)),
    UNIQUE (WorldID, OrganizationKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE WorldVenues
(
    WorldVenueID   INTEGER PRIMARY KEY,
    WorldID        INTEGER NOT NULL,
    CoreVenueKey   TEXT,
    VenueKey       TEXT NOT NULL,
    VenueName      TEXT NOT NULL,
    City           TEXT,
    StateRegion    TEXT,
    CountryCode    TEXT,
    Latitude       REAL,
    Longitude      REAL,
    TimeZoneID     TEXT,
    StadiumType    TEXT,
    TurfType       TEXT,
    Active         INTEGER NOT NULL DEFAULT 1 CHECK (Active IN (0, 1)),
    UNIQUE (WorldID, VenueKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE TeamSeasons
(
    TeamSeasonID             INTEGER PRIMARY KEY,
    CompetitionSeasonID      INTEGER NOT NULL,
    WorldOrganizationID      INTEGER NOT NULL,
    CoreTeamKey              TEXT,
    OriginCoreTeamSeasonKey  TEXT,
    DisplayName              TEXT NOT NULL,
    MarketName               TEXT,
    Nickname                 TEXT,
    Abbreviation             TEXT,
    WorldVenueID             INTEGER,
    Status                   TEXT NOT NULL DEFAULT 'ACTIVE',
    UNIQUE (CompetitionSeasonID, WorldOrganizationID),
    FOREIGN KEY (CompetitionSeasonID)
        REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (WorldOrganizationID)
        REFERENCES WorldOrganizations(WorldOrganizationID),
    FOREIGN KEY (WorldVenueID)
        REFERENCES WorldVenues(WorldVenueID)
);

CREATE TABLE TeamGroupMemberships
(
    MembershipID       INTEGER PRIMARY KEY,
    TeamSeasonID       INTEGER NOT NULL,
    WorldGroupID       INTEGER NOT NULL,
    MembershipRole     TEXT,
    PrimaryMembership  INTEGER NOT NULL DEFAULT 0
                       CHECK (PrimaryMembership IN (0, 1)),
    UNIQUE (TeamSeasonID, WorldGroupID),
    FOREIGN KEY (TeamSeasonID) REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (WorldGroupID) REFERENCES WorldCompetitionGroups(WorldGroupID)
);

CREATE TABLE People
(
    PersonID        INTEGER PRIMARY KEY,
    WorldID         INTEGER NOT NULL,
    PersonKey       TEXT NOT NULL,
    CorePersonKey   TEXT,
    FirstName       TEXT,
    LastName        TEXT,
    DisplayName     TEXT NOT NULL,
    BirthDate       TEXT,
    DeathDate       TEXT,
    Active          INTEGER NOT NULL DEFAULT 1 CHECK (Active IN (0, 1)),
    UNIQUE (WorldID, PersonKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE TABLE WorldPlayers
(
    WorldPlayerID  INTEGER PRIMARY KEY,
    PersonID       INTEGER NOT NULL UNIQUE,
    PlayerKey      TEXT,
    FOREIGN KEY (PersonID) REFERENCES People(PersonID)
);

CREATE TABLE WorldPlayerSeasons
(
    WorldPlayerSeasonID  INTEGER PRIMARY KEY,
    WorldPlayerID        INTEGER NOT NULL,
    CompetitionSeasonID  INTEGER NOT NULL,
    CorePlayerKey        TEXT,
    PositionCode         TEXT NOT NULL,
    JerseyNumber         INTEGER,
    CardType             TEXT,
    EligibilityStatus    TEXT,
    UNIQUE (WorldPlayerID, CompetitionSeasonID),
    FOREIGN KEY (WorldPlayerID) REFERENCES WorldPlayers(WorldPlayerID),
    FOREIGN KEY (CompetitionSeasonID)
        REFERENCES CompetitionSeasons(CompetitionSeasonID)
);

CREATE TABLE RosterAssignments
(
    RosterAssignmentID   INTEGER PRIMARY KEY,
    WorldPlayerSeasonID  INTEGER NOT NULL,
    TeamSeasonID         INTEGER NOT NULL,
    RosterStatus         TEXT NOT NULL,
    StartDateUtc         TEXT,
    EndDateUtc           TEXT,
    StartWeek            INTEGER,
    EndWeek              INTEGER,
    FOREIGN KEY (WorldPlayerSeasonID)
        REFERENCES WorldPlayerSeasons(WorldPlayerSeasonID),
    FOREIGN KEY (TeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID)
);

CREATE TABLE WorldScheduledGames
(
    WorldScheduledGameID  INTEGER PRIMARY KEY,
    CompetitionSeasonID   INTEGER NOT NULL,
    ScheduleKey            TEXT NOT NULL,
    StageCode              TEXT NOT NULL,
    WeekNumber             INTEGER,
    RoundCode              TEXT,
    GameDateLabel          TEXT,
    DayLabel               TEXT,
    TimeLabel              TEXT,
    ScheduledUtc           TEXT,
    AwayTeamSeasonID       INTEGER NOT NULL,
    HomeTeamSeasonID       INTEGER NOT NULL,
    WorldVenueID           INTEGER,
    SiteCode               TEXT NOT NULL,
    Status                 TEXT NOT NULL,
    OriginCoreGameKey      TEXT,
    CreatedFromTemplate    INTEGER NOT NULL DEFAULT 0
                           CHECK (CreatedFromTemplate IN (0, 1)),
    UNIQUE (CompetitionSeasonID, ScheduleKey),
    FOREIGN KEY (CompetitionSeasonID)
        REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (AwayTeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (HomeTeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (WorldVenueID)
        REFERENCES WorldVenues(WorldVenueID)
);

CREATE TABLE Games
(
    GameID                     INTEGER PRIMARY KEY,
    GameKey                    TEXT NOT NULL UNIQUE,
    WorldID                    INTEGER NOT NULL,
    CompetitionSeasonID        INTEGER,
    WorldScheduledGameID       INTEGER,
    AwayTeamSeasonID           INTEGER NOT NULL,
    HomeTeamSeasonID           INTEGER NOT NULL,
    WorldVenueID               INTEGER,
    GameStatus                 TEXT NOT NULL,
    ResolutionMode             TEXT NOT NULL,
    SimulationProviderKey      TEXT NOT NULL,
    SimulationProviderVersion  TEXT NOT NULL,
    StartedUtc                 TEXT,
    CompletedUtc               TEXT,
    AwayScore                  INTEGER,
    HomeScore                  INTEGER,
    WinnerTeamSeasonID         INTEGER,
    HistoricalGameKey          TEXT,
    ProviderCoverageKey        TEXT,
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID),
    FOREIGN KEY (CompetitionSeasonID)
        REFERENCES CompetitionSeasons(CompetitionSeasonID),
    FOREIGN KEY (WorldScheduledGameID)
        REFERENCES WorldScheduledGames(WorldScheduledGameID),
    FOREIGN KEY (AwayTeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (HomeTeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID),
    FOREIGN KEY (WorldVenueID)
        REFERENCES WorldVenues(WorldVenueID),
    FOREIGN KEY (WinnerTeamSeasonID)
        REFERENCES TeamSeasons(TeamSeasonID)
);

CREATE TABLE GameArtifacts
(
    GameArtifactID    INTEGER PRIMARY KEY,
    GameID            INTEGER NOT NULL,
    ArtifactType      TEXT NOT NULL,
    RelativePath      TEXT NOT NULL,
    FormatKey         TEXT,
    FormatVersion     TEXT,
    HashAlgorithm     TEXT,
    ContentHash       TEXT,
    CreatedUtc        TEXT NOT NULL,
    UNIQUE (GameID, ArtifactType, RelativePath),
    FOREIGN KEY (GameID) REFERENCES Games(GameID)
);

CREATE TABLE PlayerInjuries
(
    InjuryID              INTEGER PRIMARY KEY,
    WorldPlayerSeasonID   INTEGER NOT NULL,
    GameID                INTEGER,
    PlayID                INTEGER,
    OccurredUtc            TEXT,
    InjuryCode             TEXT,
    GamesRemaining         INTEGER,
    ExpectedReturnUtc      TEXT,
    Status                 TEXT NOT NULL,
    ResolvedUtc            TEXT,
    FOREIGN KEY (WorldPlayerSeasonID)
        REFERENCES WorldPlayerSeasons(WorldPlayerSeasonID),
    FOREIGN KEY (GameID) REFERENCES Games(GameID)
);

CREATE TABLE PlayerState
(
    WorldPlayerSeasonID  INTEGER PRIMARY KEY,
    HealthStatus         TEXT NOT NULL,
    FatigueState         TEXT,
    Active               INTEGER NOT NULL CHECK (Active IN (0, 1)),
    CurrentInjuryID      INTEGER,
    FOREIGN KEY (WorldPlayerSeasonID)
        REFERENCES WorldPlayerSeasons(WorldPlayerSeasonID),
    FOREIGN KEY (CurrentInjuryID)
        REFERENCES PlayerInjuries(InjuryID)
);

CREATE TABLE PlayerRatingModifiers
(
    ModifierID           INTEGER PRIMARY KEY,
    WorldPlayerSeasonID  INTEGER NOT NULL,
    RatingCode           TEXT NOT NULL,
    Adjustment           REAL NOT NULL,
    EffectiveUtc         TEXT,
    EndUtc               TEXT,
    ReasonCode           TEXT NOT NULL,
    SourceEventRef       TEXT,
    FOREIGN KEY (WorldPlayerSeasonID)
        REFERENCES WorldPlayerSeasons(WorldPlayerSeasonID)
);

CREATE TABLE GameState
(
    GameID                     INTEGER PRIMARY KEY,
    Quarter                    INTEGER NOT NULL,
    ClockSeconds               INTEGER NOT NULL,
    PossessionTeamID           INTEGER,
    FieldPosition              INTEGER NOT NULL,
    Down                       INTEGER NOT NULL,
    Distance                   INTEGER NOT NULL,
    AwayTimeouts               INTEGER NOT NULL,
    HomeTimeouts               INTEGER NOT NULL,
    CurrentDriveID             INTEGER,
    NextPlayNumber             INTEGER NOT NULL,
    PresentationRevealCursor   TEXT,
    FOREIGN KEY (GameID) REFERENCES Games(GameID),
    FOREIGN KEY (PossessionTeamID)
        REFERENCES TeamSeasons(TeamSeasonID)
);

CREATE TABLE RandomStreams
(
    RandomStreamID     INTEGER PRIMARY KEY,
    WorldID            INTEGER NOT NULL,
    StreamKey          TEXT NOT NULL,
    ScopeType          TEXT NOT NULL,
    ScopeKey           TEXT NOT NULL,
    AlgorithmKey       TEXT NOT NULL,
    AlgorithmVersion   TEXT NOT NULL,
    SeedMaterial       TEXT NOT NULL,
    StateData          TEXT,
    NextOrdinal        INTEGER NOT NULL DEFAULT 0,
    UNIQUE (WorldID, StreamKey, ScopeType, ScopeKey),
    FOREIGN KEY (WorldID) REFERENCES Worlds(WorldID)
);

CREATE INDEX IX_WorldCoreSnapshots_World
    ON WorldCoreSnapshots(WorldID);

CREATE INDEX IX_WorldCompetitions_World
    ON WorldCompetitions(WorldID);

CREATE INDEX IX_CompetitionSeasons_Competition
    ON CompetitionSeasons(WorldCompetitionID);

CREATE INDEX IX_WorldCompetitionGroups_Season
    ON WorldCompetitionGroups(CompetitionSeasonID);

CREATE INDEX IX_WorldCompetitionGroups_Parent
    ON WorldCompetitionGroups(ParentWorldGroupID);

CREATE INDEX IX_WorldOrganizations_World
    ON WorldOrganizations(WorldID);

CREATE INDEX IX_WorldVenues_World
    ON WorldVenues(WorldID);

CREATE INDEX IX_TeamSeasons_CompetitionSeason
    ON TeamSeasons(CompetitionSeasonID);

CREATE INDEX IX_TeamSeasons_Organization
    ON TeamSeasons(WorldOrganizationID);

CREATE INDEX IX_TeamGroupMemberships_Team
    ON TeamGroupMemberships(TeamSeasonID);

CREATE INDEX IX_TeamGroupMemberships_Group
    ON TeamGroupMemberships(WorldGroupID);

CREATE INDEX IX_People_World
    ON People(WorldID);

CREATE INDEX IX_WorldPlayerSeasons_CompetitionSeason
    ON WorldPlayerSeasons(CompetitionSeasonID);

CREATE INDEX IX_RosterAssignments_PlayerSeason
    ON RosterAssignments(WorldPlayerSeasonID);

CREATE INDEX IX_RosterAssignments_TeamSeason
    ON RosterAssignments(TeamSeasonID);

CREATE INDEX IX_RosterAssignments_Active
    ON RosterAssignments(TeamSeasonID, EndDateUtc);

CREATE INDEX IX_WorldScheduledGames_SeasonKickoff
    ON WorldScheduledGames(CompetitionSeasonID, ScheduledUtc, ScheduleKey);

CREATE INDEX IX_WorldScheduledGames_SeasonWeek
    ON WorldScheduledGames(CompetitionSeasonID, StageCode, WeekNumber, Status);

CREATE INDEX IX_WorldScheduledGames_AwayTeam
    ON WorldScheduledGames(AwayTeamSeasonID);

CREATE INDEX IX_WorldScheduledGames_HomeTeam
    ON WorldScheduledGames(HomeTeamSeasonID);

CREATE INDEX IX_Games_CompetitionSeason
    ON Games(CompetitionSeasonID);

CREATE INDEX IX_Games_ScheduledGame
    ON Games(WorldScheduledGameID);

CREATE INDEX IX_Games_CompletedUtc
    ON Games(CompletedUtc);

CREATE INDEX IX_GameArtifacts_Game
    ON GameArtifacts(GameID);

CREATE INDEX IX_PlayerInjuries_PlayerSeason
    ON PlayerInjuries(WorldPlayerSeasonID, Status);

CREATE INDEX IX_PlayerRatingModifiers_PlayerRating
    ON PlayerRatingModifiers(
        WorldPlayerSeasonID,
        RatingCode,
        EffectiveUtc,
        EndUtc);

CREATE UNIQUE INDEX UX_Games_ScheduledGame_NonVoid
    ON Games(WorldScheduledGameID)
    WHERE WorldScheduledGameID IS NOT NULL
      AND GameStatus <> 'VOID';
)SQL");
    }

    // -------------------------------------------------------------------------
    // Core -> World creation data
    // -------------------------------------------------------------------------

    struct CoreCompetitionSelection
    {
        std::int64_t coreCompetitionSeasonId = 0;
        std::string competitionKey;
        std::string competitionName;
    };

    struct CoreGroupRow
    {
        std::int64_t coreGroupId = 0;
        std::optional<std::int64_t> parentCoreGroupId;
        std::string groupType;
        std::string groupKey;
        std::string displayName;
    };

    struct CoreScheduleRow
    {
        std::string sourceGameId;
        std::string stage;
        std::int32_t weekNumber = 0;
        std::string gameDate;
        std::string dayLabel;
        std::string timeLabel;
        std::int64_t awayCoreTeamSeasonId = 0;
        std::int64_t homeCoreTeamSeasonId = 0;
        std::string site;
        std::string scheduledUtc;
    };

    CoreCompetitionSelection selectCoreCompetition(
        sqlite3* core,
        std::int32_t season)
    {
        Statement statement(
            core,
            R"SQL(
SELECT
    cs.CompetitionSeasonID,
    c.CompetitionKey,
    c.DisplayName
FROM CompetitionSeasons cs
JOIN Competitions c
    ON c.CompetitionID = cs.CompetitionID
WHERE cs.SeasonYear = ?
ORDER BY c.CompetitionKey;
)SQL");

        bindInt(statement.get(), 1, season);

        std::vector<CoreCompetitionSelection> matches;
        while (statement.stepRow(core))
        {
            CoreCompetitionSelection value;
            value.coreCompetitionSeasonId =
                sqlite3_column_int64(statement.get(), 0);
            value.competitionKey = columnText(statement.get(), 1);
            value.competitionName = columnText(statement.get(), 2);
            matches.push_back(std::move(value));
        }

        if (matches.size() != 1)
        {
            throw WorldDatabaseException(
                "WorldDB v1 requires exactly one Core competition season for "
                "the requested season. Found " +
                std::to_string(matches.size()) + ".");
        }

        return matches.front();
    }

    std::vector<CoreScheduleRow> loadCoreRegularScheduleRows(
        sqlite3* core,
        std::int64_t coreCompetitionSeasonId,
        const std::string& scheduleClockZoneId)
    {
        Statement statement(
            core,
            R"SQL(
SELECT
    SourceGameID,
    Stage,
    WeekNumber,
    GameDate,
    COALESCE(DayLabel, ''),
    COALESCE(TimeLabel, ''),
    AwayTeamSeasonID,
    HomeTeamSeasonID,
    Site
FROM ScheduleGames
WHERE CompetitionSeasonID = ?
  AND Stage = 'REGULAR'
  AND WeekNumber IS NOT NULL
ORDER BY WeekNumber, GameDate, TimeLabel, SourceGameID;
)SQL");

        bindInt64(statement.get(), 1, coreCompetitionSeasonId);

        std::vector<CoreScheduleRow> rows;
        while (statement.stepRow(core))
        {
            CoreScheduleRow row;
            row.sourceGameId = columnText(statement.get(), 0);
            row.stage = columnText(statement.get(), 1);
            row.weekNumber = sqlite3_column_int(statement.get(), 2);
            row.gameDate = columnText(statement.get(), 3);
            row.dayLabel = columnText(statement.get(), 4);
            row.timeLabel = columnText(statement.get(), 5);
            row.awayCoreTeamSeasonId = sqlite3_column_int64(statement.get(), 6);
            row.homeCoreTeamSeasonId = sqlite3_column_int64(statement.get(), 7);
            row.site = columnText(statement.get(), 8);

            if (row.sourceGameId.empty() || row.gameDate.empty() ||
                row.timeLabel.empty())
            {
                throw WorldDatabaseException(
                    "Core regular-season schedule contains incomplete identity/time data.");
            }

            row.scheduledUtc =
                localToUtc(scheduleClockZoneId, row.gameDate, row.timeLabel);

            rows.push_back(std::move(row));
        }

        if (rows.empty())
        {
            throw WorldDatabaseException(
                "Core contains no regular-season schedule rows for the selected season.");
        }

        return rows;
    }

    std::string initialWorldTimeUtc(
        const std::vector<CoreScheduleRow>& schedule,
        const std::string& scheduleClockZoneId)
    {
        std::string earliestDate = schedule.front().gameDate;
        for (const CoreScheduleRow& row : schedule)
        {
            if (row.gameDate < earliestDate)
            {
                earliestDate = row.gameDate;
            }
        }

        return localToUtc(scheduleClockZoneId, earliestDate, "12:00AM");
    }

    std::int32_t initialWeek(const std::vector<CoreScheduleRow>& schedule)
    {
        std::int32_t week = schedule.front().weekNumber;
        for (const CoreScheduleRow& row : schedule)
        {
            week = std::min(week, row.weekNumber);
        }
        return week;
    }

    struct CreationContext
    {
        sqlite3* core = nullptr;
        sqlite3* world = nullptr;
        WorldDatabaseCreateResult* result = nullptr;

        std::int64_t worldId = 1;
        std::int64_t worldCoreSnapshotId = 0;
        std::int64_t competitionSeasonId = 0;

        std::unordered_map<std::int64_t, std::int64_t> groupByCoreId;
        std::unordered_map<std::int64_t, std::int64_t> venueByCoreId;
        std::unordered_map<std::int64_t, std::int64_t> teamSeasonByCoreId;
        std::unordered_map<std::int64_t, std::int64_t> teamVenueByCoreTeamSeasonId;

        std::size_t groupsCreated = 0;
        std::size_t venuesCreated = 0;
        std::size_t membershipsCreated = 0;
        std::size_t worldPlayerSeasonsCreated = 0;
        std::size_t playerStateCreated = 0;
    };

    std::int64_t insertWorldCompetitionAndSeason(
        CreationContext& context,
        const CoreCompetitionSelection& source,
        const std::string& ruleset,
        const std::string& scheduleClockZoneId,
        std::int32_t season)
    {
        Statement competitionInsert(
            context.world,
            R"SQL(
INSERT INTO WorldCompetitions
(
    WorldID, CompetitionKey, Name, CompetitionType, LevelCode, Active
)
VALUES(?, ?, ?, 'UNSPECIFIED', NULL, 1);
)SQL");

        bindInt64(competitionInsert.get(), 1, context.worldId);
        bindText(competitionInsert.get(), 2, source.competitionKey);
        bindText(competitionInsert.get(), 3, source.competitionName);
        competitionInsert.stepDone(context.world);

        const std::int64_t worldCompetitionId = lastInsertId(context.world);
        ++context.result->competitionsCreated;

        const std::string seasonKey =
            source.competitionKey + "_" + std::to_string(season);

        Statement seasonInsert(
            context.world,
            R"SQL(
INSERT INTO CompetitionSeasons
(
    WorldCompetitionID,
    WorldCoreSnapshotID,
    SeasonKey,
    SeasonLabel,
    SeasonYear,
    StartDateUtc,
    EndDateUtc,
    Status,
    CurrentPhaseCode,
    SimulationRulesProfileKey,
    GameRulesProfileKey,
    CompetitionPolicyProfileKey,
    DefaultSimulationProviderKey,
    ScheduleClockZoneId
)
VALUES(?, ?, ?, ?, ?, NULL, NULL, 'ACTIVE', 'REGULAR_SEASON', ?,
       NULL, NULL, NULL, ?);
)SQL");

        bindInt64(seasonInsert.get(), 1, worldCompetitionId);
        bindInt64(seasonInsert.get(), 2, context.worldCoreSnapshotId);
        bindText(seasonInsert.get(), 3, seasonKey);
        bindText(seasonInsert.get(), 4, std::to_string(season));
        bindInt(seasonInsert.get(), 5, season);
        bindText(seasonInsert.get(), 6, ruleset);
        bindText(seasonInsert.get(), 7, scheduleClockZoneId);
        seasonInsert.stepDone(context.world);

        ++context.result->competitionSeasonsCreated;
        context.competitionSeasonId = lastInsertId(context.world);
        return context.competitionSeasonId;
    }

    void instantiateGroups(
        CreationContext& context,
        std::int64_t coreCompetitionSeasonId)
    {
        Statement query(
            context.core,
            R"SQL(
SELECT
    CompetitionGroupID,
    ParentGroupID,
    GroupType,
    GroupKey,
    DisplayName
FROM CompetitionGroups
WHERE CompetitionSeasonID = ?
ORDER BY CompetitionGroupID;
)SQL");

        bindInt64(query.get(), 1, coreCompetitionSeasonId);

        std::vector<CoreGroupRow> pending;
        while (query.stepRow(context.core))
        {
            CoreGroupRow row;
            row.coreGroupId = sqlite3_column_int64(query.get(), 0);
            row.parentCoreGroupId = columnOptionalInt64(query.get(), 1);
            row.groupType = columnText(query.get(), 2);
            row.groupKey = columnText(query.get(), 3);
            row.displayName = columnText(query.get(), 4);
            pending.push_back(std::move(row));
        }

        Statement insert(
            context.world,
            R"SQL(
INSERT INTO WorldCompetitionGroups
(
    CompetitionSeasonID,
    ParentWorldGroupID,
    HierarchyKey,
    GroupKey,
    GroupType,
    GroupName,
    SortOrder
)
VALUES(?, ?, 'PRIMARY', ?, ?, ?, ?);
)SQL");

        std::size_t inserted = 0;
        int sortOrder = 0;

        while (inserted < pending.size())
        {
            bool madeProgress = false;

            for (const CoreGroupRow& row : pending)
            {
                if (context.groupByCoreId.find(row.coreGroupId) !=
                    context.groupByCoreId.end())
                {
                    continue;
                }

                std::optional<std::int64_t> parentWorldId;
                if (row.parentCoreGroupId.has_value())
                {
                    const auto parent =
                        context.groupByCoreId.find(*row.parentCoreGroupId);

                    if (parent == context.groupByCoreId.end())
                    {
                        continue;
                    }

                    parentWorldId = parent->second;
                }

                insert.reset();
                bindInt64(insert.get(), 1, context.competitionSeasonId);
                bindOptionalInt64(insert.get(), 2, parentWorldId);
                bindText(insert.get(), 3, row.groupKey);
                bindText(insert.get(), 4, row.groupType);
                bindText(insert.get(), 5, row.displayName);
                bindInt(insert.get(), 6, ++sortOrder);
                insert.stepDone(context.world);

                context.groupByCoreId[row.coreGroupId] =
                    lastInsertId(context.world);

                ++inserted;
                ++context.groupsCreated;
                madeProgress = true;
            }

            if (!madeProgress)
            {
                throw WorldDatabaseException(
                    "Core competition-group hierarchy contains an unresolved "
                    "parent reference or cycle.");
            }
        }
    }

    void instantiateVenues(CreationContext& context)
    {
        Statement query(
            context.core,
            R"SQL(
SELECT
    VenueID,
    VenueKey,
    Name,
    COALESCE(City, ''),
    COALESCE(State, ''),
    COALESCE(LatitudeText, ''),
    COALESCE(LongitudeText, ''),
    COALESCE(TimeZone, ''),
    COALESCE(Surface, ''),
    COALESCE(StadiumType, '')
FROM Venues
ORDER BY VenueID;
)SQL");

        Statement insert(
            context.world,
            R"SQL(
INSERT INTO WorldVenues
(
    WorldID,
    CoreVenueKey,
    VenueKey,
    VenueName,
    City,
    StateRegion,
    CountryCode,
    Latitude,
    Longitude,
    TimeZoneID,
    StadiumType,
    TurfType,
    Active
)
VALUES(?, ?, ?, ?, ?, ?, NULL, ?, ?, ?, ?, ?, 1);
)SQL");

        while (query.stepRow(context.core))
        {
            const std::int64_t coreVenueId =
                sqlite3_column_int64(query.get(), 0);

            const std::string venueKey = columnText(query.get(), 1);
            const std::string name = columnText(query.get(), 2);
            const std::string city = columnText(query.get(), 3);
            const std::string state = columnText(query.get(), 4);
            const std::string latitude = columnText(query.get(), 5);
            const std::string longitude = columnText(query.get(), 6);
            const std::string timeZone = columnText(query.get(), 7);
            const std::string surface = columnText(query.get(), 8);
            const std::string stadiumType = columnText(query.get(), 9);

            const std::optional<double> latitudeValue =
                parseOptionalDouble(latitude);
            const std::optional<double> longitudeValue =
                parseOptionalDouble(longitude);

            insert.reset();
            bindInt64(insert.get(), 1, context.worldId);
            bindText(insert.get(), 2, venueKey);
            bindText(insert.get(), 3, venueKey);
            bindText(insert.get(), 4, name);
            bindOptionalText(insert.get(), 5, city);
            bindOptionalText(insert.get(), 6, state);

            if (latitudeValue.has_value())
                bindDouble(insert.get(), 7, *latitudeValue);
            else
                bindNull(insert.get(), 7);

            if (longitudeValue.has_value())
                bindDouble(insert.get(), 8, *longitudeValue);
            else
                bindNull(insert.get(), 8);

            bindOptionalText(insert.get(), 9, timeZone);
            bindOptionalText(insert.get(), 10, stadiumType);
            bindOptionalText(insert.get(), 11, surface);
            insert.stepDone(context.world);

            context.venueByCoreId[coreVenueId] = lastInsertId(context.world);
            ++context.venuesCreated;
        }
    }

    void instantiateTeams(
        CreationContext& context,
        std::int64_t coreCompetitionSeasonId)
    {
        Statement query(
            context.core,
            R"SQL(
SELECT
    ts.TeamSeasonID,
    t.TeamID,
    t.TeamKey,
    t.City,
    t.Nickname,
    ts.TeamSeasonKey,
    ts.ConferenceGroupID,
    ts.DivisionGroupID,
    ts.HomeVenueID
FROM TeamSeasons ts
JOIN Teams t
    ON t.TeamID = ts.TeamID
WHERE ts.CompetitionSeasonID = ?
ORDER BY ts.TeamSeasonID;
)SQL");

        bindInt64(query.get(), 1, coreCompetitionSeasonId);

        Statement orgInsert(
            context.world,
            R"SQL(
INSERT INTO WorldOrganizations
(
    WorldID, OrganizationKey, OrganizationType,
    CanonicalName, OriginCoreKey, Active
)
VALUES(?, ?, 'TEAM', ?, ?, 1);
)SQL");

        Statement teamInsert(
            context.world,
            R"SQL(
INSERT INTO TeamSeasons
(
    CompetitionSeasonID,
    WorldOrganizationID,
    CoreTeamKey,
    OriginCoreTeamSeasonKey,
    DisplayName,
    MarketName,
    Nickname,
    Abbreviation,
    WorldVenueID,
    Status
)
VALUES(?, ?, ?, ?, ?, ?, ?, NULL, ?, 'ACTIVE');
)SQL");

        Statement membershipInsert(
            context.world,
            R"SQL(
INSERT INTO TeamGroupMemberships
(
    TeamSeasonID, WorldGroupID, MembershipRole, PrimaryMembership
)
VALUES(?, ?, ?, ?);
)SQL");

        while (query.stepRow(context.core))
        {
            const std::int64_t coreTeamSeasonId =
                sqlite3_column_int64(query.get(), 0);
            const std::string teamKey = columnText(query.get(), 2);
            const std::string city = columnText(query.get(), 3);
            const std::string nickname = columnText(query.get(), 4);
            const std::string teamSeasonKey = columnText(query.get(), 5);
            const auto conferenceCoreId = columnOptionalInt64(query.get(), 6);
            const auto divisionCoreId = columnOptionalInt64(query.get(), 7);
            const auto homeVenueCoreId = columnOptionalInt64(query.get(), 8);

            std::string canonicalName;
            if (city.empty()) canonicalName = nickname;
            else if (nickname.empty()) canonicalName = city;
            else canonicalName = city + " " + nickname;

            orgInsert.reset();
            bindInt64(orgInsert.get(), 1, context.worldId);
            bindText(orgInsert.get(), 2, teamKey);
            bindText(orgInsert.get(), 3, canonicalName);
            bindText(orgInsert.get(), 4, teamKey);
            orgInsert.stepDone(context.world);

            const std::int64_t worldOrganizationId =
                lastInsertId(context.world);
            ++context.result->organizationsCreated;

            std::optional<std::int64_t> worldVenueId;
            if (homeVenueCoreId.has_value())
            {
                const auto venue = context.venueByCoreId.find(*homeVenueCoreId);
                if (venue == context.venueByCoreId.end())
                {
                    throw WorldDatabaseException(
                        "Core TeamSeason references an unknown HomeVenueID.");
                }
                worldVenueId = venue->second;
            }

            teamInsert.reset();
            bindInt64(teamInsert.get(), 1, context.competitionSeasonId);
            bindInt64(teamInsert.get(), 2, worldOrganizationId);
            bindText(teamInsert.get(), 3, teamKey);
            bindText(teamInsert.get(), 4, teamSeasonKey);
            bindText(teamInsert.get(), 5, canonicalName);
            bindOptionalText(teamInsert.get(), 6, city);
            bindOptionalText(teamInsert.get(), 7, nickname);
            bindOptionalInt64(teamInsert.get(), 8, worldVenueId);
            teamInsert.stepDone(context.world);

            const std::int64_t worldTeamSeasonId = lastInsertId(context.world);
            context.teamSeasonByCoreId[coreTeamSeasonId] = worldTeamSeasonId;
            context.teamVenueByCoreTeamSeasonId[coreTeamSeasonId] =
                worldVenueId.value_or(0);
            ++context.result->teamSeasonsCreated;

            auto addMembership =
                [&](const std::optional<std::int64_t>& coreGroupId,
                    const char* role,
                    bool primary)
                {
                    if (!coreGroupId.has_value())
                    {
                        return;
                    }

                    const auto group =
                        context.groupByCoreId.find(*coreGroupId);

                    if (group == context.groupByCoreId.end())
                    {
                        throw WorldDatabaseException(
                            "Core TeamSeason references an unknown competition group.");
                    }

                    membershipInsert.reset();
                    bindInt64(membershipInsert.get(), 1, worldTeamSeasonId);
                    bindInt64(membershipInsert.get(), 2, group->second);
                    bindText(membershipInsert.get(), 3, role);
                    bindInt(membershipInsert.get(), 4, primary ? 1 : 0);
                    membershipInsert.stepDone(context.world);
                    ++context.membershipsCreated;
                };

            addMembership(conferenceCoreId, "CONFERENCE", false);
            addMembership(divisionCoreId, "DIVISION", true);
        }
    }

    #include "GCE/Database/WorldTeamDataBuild.inl"
    #include "GCE/Database/ClockSchedulerBuild.inl"

    void instantiatePlayers(
        CreationContext& context,
        std::int64_t coreCompetitionSeasonId,
        const std::string& initialWorldUtc,
        std::int32_t firstWeek)
    {
        Statement query(
            context.core,
            R"SQL(
SELECT
    p.PlayerID,
    p.SourceLocalKey,
    p.TeamSeasonID,
    p.FirstName,
    p.LastName,
    p.Jersey,
    p.Position,
    COALESCE(prp.CardType, '')
FROM Players p
JOIN TeamSeasons ts
    ON ts.TeamSeasonID = p.TeamSeasonID
LEFT JOIN PlayerRatingProfiles prp
    ON prp.PlayerID = p.PlayerID
WHERE ts.CompetitionSeasonID = ?
ORDER BY p.PlayerID;
)SQL");

        bindInt64(query.get(), 1, coreCompetitionSeasonId);

        Statement playerInsert(
            context.world,
            R"SQL(
INSERT INTO WorldPlayers(PersonID, PlayerKey)
VALUES(?, ?);
)SQL");

        Statement playerSeasonInsert(
            context.world,
            R"SQL(
INSERT INTO WorldPlayerSeasons
(
    WorldPlayerID,
    CompetitionSeasonID,
    CorePlayerKey,
    PositionCode,
    JerseyNumber,
    CardType,
    EligibilityStatus
)
VALUES(?, ?, ?, ?, ?, ?, NULL);
)SQL");

        Statement rosterInsert(
            context.world,
            R"SQL(
INSERT INTO RosterAssignments
(
    WorldPlayerSeasonID,
    TeamSeasonID,
    RosterStatus,
    StartDateUtc,
    EndDateUtc,
    StartWeek,
    EndWeek
)
VALUES(?, ?, 'ACTIVE', ?, NULL, ?, NULL);
)SQL");

        Statement stateInsert(
            context.world,
            R"SQL(
INSERT INTO PlayerState
(
    WorldPlayerSeasonID,
    HealthStatus,
    FatigueState,
    Active,
    CurrentInjuryID
)
VALUES(?, 'HEALTHY', NULL, 1, NULL);
)SQL");

        while (query.stepRow(context.core))
        {
            const std::string sourceKey = columnText(query.get(), 1);
            const std::int64_t coreTeamSeasonId =
                sqlite3_column_int64(query.get(), 2);
            const std::string firstName = columnText(query.get(), 3);
            const std::string lastName = columnText(query.get(), 4);
            const std::int32_t jersey = sqlite3_column_int(query.get(), 5);
            const std::string position = columnText(query.get(), 6);
            const std::string cardType = columnText(query.get(), 7);

            const auto team =
                context.teamSeasonByCoreId.find(coreTeamSeasonId);

            if (team == context.teamSeasonByCoreId.end())
            {
                throw WorldDatabaseException(
                    "Core Player references a TeamSeason that was not instantiated.");
            }

            const auto corePlayerId = sqlite3_column_int64(query.get(), 0);
            TeamDataSql::Query identity(context.core, "SELECT CorePersonKey FROM CorePlayerPeople WHERE PlayerID=?", {static_cast<std::int64_t>(corePlayerId)});
            if (!identity.row()) throw WorldDatabaseException("Core player has no explicit identity mapping");
            const std::int64_t personId = TeamDataSql::scalar(context.world,
                "SELECT PersonID FROM WorldPersonSourceAliases WHERE WorldID=? AND WorldCoreSnapshotID=? AND CorePersonKey=?",
                {context.worldId,context.worldCoreSnapshotId,identity.text()});

            playerInsert.reset();
            bindInt64(playerInsert.get(), 1, personId);
            bindText(playerInsert.get(), 2, sourceKey);
            playerInsert.stepDone(context.world);

            const std::int64_t worldPlayerId = lastInsertId(context.world);
            ++context.result->worldPlayersCreated;

            playerSeasonInsert.reset();
            bindInt64(playerSeasonInsert.get(), 1, worldPlayerId);
            bindInt64(playerSeasonInsert.get(), 2, context.competitionSeasonId);
            bindText(playerSeasonInsert.get(), 3, sourceKey);
            bindText(playerSeasonInsert.get(), 4, position);
            bindInt(playerSeasonInsert.get(), 5, jersey);
            bindOptionalText(playerSeasonInsert.get(), 6, cardType);
            playerSeasonInsert.stepDone(context.world);

            const std::int64_t worldPlayerSeasonId =
                lastInsertId(context.world);
            ++context.worldPlayerSeasonsCreated;

            rosterInsert.reset();
            bindInt64(rosterInsert.get(), 1, worldPlayerSeasonId);
            bindInt64(rosterInsert.get(), 2, team->second);
            bindText(rosterInsert.get(), 3, initialWorldUtc);
            bindInt(rosterInsert.get(), 4, firstWeek);
            rosterInsert.stepDone(context.world);
            ++context.result->rosterAssignmentsCreated;

            stateInsert.reset();
            bindInt64(stateInsert.get(), 1, worldPlayerSeasonId);
            stateInsert.stepDone(context.world);
            ++context.playerStateCreated;
        }
    }

    void instantiateSchedule(
        CreationContext& context,
        const std::vector<CoreScheduleRow>& schedule)
    {
        Statement insert(
            context.world,
            R"SQL(
INSERT INTO WorldScheduledGames
(
    CompetitionSeasonID,
    ScheduleKey,
    StageCode,
    WeekNumber,
    RoundCode,
    GameDateLabel,
    DayLabel,
    TimeLabel,
    ScheduledUtc,
    AwayTeamSeasonID,
    HomeTeamSeasonID,
    WorldVenueID,
    SiteCode,
    Status,
    OriginCoreGameKey,
    CreatedFromTemplate
)
VALUES(?, ?, ?, ?, NULL, ?, ?, ?, ?, ?, ?, ?, ?, 'SCHEDULED', ?, 1);
)SQL");

        std::string earliestKickoff;
        std::string latestKickoff;

        for (const CoreScheduleRow& row : schedule)
        {
            const auto away =
                context.teamSeasonByCoreId.find(row.awayCoreTeamSeasonId);
            const auto home =
                context.teamSeasonByCoreId.find(row.homeCoreTeamSeasonId);

            if (away == context.teamSeasonByCoreId.end() ||
                home == context.teamSeasonByCoreId.end())
            {
                throw WorldDatabaseException(
                    "Core schedule references a TeamSeason that was not instantiated.");
            }

            std::optional<std::int64_t> worldVenueId;

            if (row.site == "HOME")
            {
                const auto venue = context.teamVenueByCoreTeamSeasonId.find(
                    row.homeCoreTeamSeasonId);
                if (venue != context.teamVenueByCoreTeamSeasonId.end() &&
                    venue->second != 0)
                {
                    worldVenueId = venue->second;
                }
            }
            else if (row.site == "AWAY")
            {
                const auto venue = context.teamVenueByCoreTeamSeasonId.find(
                    row.awayCoreTeamSeasonId);
                if (venue != context.teamVenueByCoreTeamSeasonId.end() &&
                    venue->second != 0)
                {
                    worldVenueId = venue->second;
                }
            }

            insert.reset();
            bindInt64(insert.get(), 1, context.competitionSeasonId);
            bindText(insert.get(), 2, row.sourceGameId);
            bindText(insert.get(), 3, row.stage);
            bindInt(insert.get(), 4, row.weekNumber);
            bindText(insert.get(), 5, row.gameDate);
            bindOptionalText(insert.get(), 6, row.dayLabel);
            bindOptionalText(insert.get(), 7, row.timeLabel);
            bindText(insert.get(), 8, row.scheduledUtc);
            bindInt64(insert.get(), 9, away->second);
            bindInt64(insert.get(), 10, home->second);
            bindOptionalInt64(insert.get(), 11, worldVenueId);
            bindText(insert.get(), 12, row.site);
            bindText(insert.get(), 13, row.sourceGameId);
            insert.stepDone(context.world);

            ++context.result->scheduledGamesCreated;

            if (earliestKickoff.empty() || row.scheduledUtc < earliestKickoff)
                earliestKickoff = row.scheduledUtc;
            if (latestKickoff.empty() || row.scheduledUtc > latestKickoff)
                latestKickoff = row.scheduledUtc;
        }

        Statement updateSeason(
            context.world,
            R"SQL(
UPDATE CompetitionSeasons
SET StartDateUtc = ?, EndDateUtc = ?
WHERE CompetitionSeasonID = ?;
)SQL");

        bindText(updateSeason.get(), 1, earliestKickoff);
        bindText(updateSeason.get(), 2, latestKickoff);
        bindInt64(updateSeason.get(), 3, context.competitionSeasonId);
        updateSeason.stepDone(context.world);
    }

    void requireCount(
        sqlite3* db,
        const char* table,
        std::int64_t expected)
    {
        const std::string sql =
            "SELECT COUNT(*) FROM " + std::string(table) + ";";
        Statement statement(db, sql.c_str());
        statement.stepRow(db);
        const std::int64_t actual = sqlite3_column_int64(statement.get(), 0);

        if (actual != expected)
        {
            throw WorldDatabaseException(
                std::string("World creation parity failure for ") + table +
                ": expected " + std::to_string(expected) +
                ", found " + std::to_string(actual) + ".");
        }
    }

    void validateCreationParity(CreationContext& context)
    {
        requireCount(context.world, "Worlds", 1);
        requireCount(context.world, "WorldCoreSnapshots", 1);
        requireCount(
            context.world,
            "WorldCompetitions",
            static_cast<std::int64_t>(context.result->competitionsCreated));
        requireCount(
            context.world,
            "CompetitionSeasons",
            static_cast<std::int64_t>(context.result->competitionSeasonsCreated));
        requireCount(
            context.world,
            "WorldCompetitionGroups",
            static_cast<std::int64_t>(context.groupsCreated));
        requireCount(
            context.world,
            "WorldOrganizations",
            static_cast<std::int64_t>(context.result->organizationsCreated));
        requireCount(
            context.world,
            "TeamSeasons",
            static_cast<std::int64_t>(context.result->teamSeasonsCreated));
        requireCount(
            context.world,
            "TeamGroupMemberships",
            static_cast<std::int64_t>(context.membershipsCreated));
        requireCount(
            context.world,
            "People",
            static_cast<std::int64_t>(context.result->peopleCreated));
        requireCount(
            context.world,
            "WorldPlayers",
            static_cast<std::int64_t>(context.result->worldPlayersCreated));
        requireCount(
            context.world,
            "WorldPlayerSeasons",
            static_cast<std::int64_t>(context.worldPlayerSeasonsCreated));
        requireCount(
            context.world,
            "RosterAssignments",
            static_cast<std::int64_t>(context.result->rosterAssignmentsCreated));
        requireCount(
            context.world,
            "PlayerState",
            static_cast<std::int64_t>(context.playerStateCreated));
        requireCount(
            context.world,
            "WorldScheduledGames",
            static_cast<std::int64_t>(context.result->scheduledGamesCreated));

        requireCount(context.world, "Games", 0);
        requireCount(context.world, "GameArtifacts", 0);
        requireCount(context.world, "PlayerInjuries", 0);
        requireCount(context.world, "PlayerRatingModifiers", 0);

        if (scalarInt64(
            context.world,
            "SELECT COUNT(*) FROM WorldScheduledGames WHERE ScheduledUtc IS NULL OR ScheduledUtc = '';"
        ) != 0)
        {
            throw WorldDatabaseException(
                "World creation produced a scheduled game without canonical UTC kickoff.");
        }

        if (scalarInt64(
            context.world,
            "SELECT COUNT(*) FROM WorldScheduledGames WHERE StageCode <> 'REGULAR';"
        ) != 0)
        {
            throw WorldDatabaseException(
                "Initial World creation copied non-regular-season schedule rows.");
        }

        if (scalarInt64(
            context.world,
            R"SQL(
SELECT COUNT(*)
FROM WorldPlayerSeasons wps
WHERE (
    SELECT COUNT(*)
    FROM RosterAssignments ra
    WHERE ra.WorldPlayerSeasonID = wps.WorldPlayerSeasonID
      AND ra.EndDateUtc IS NULL
) <> 1;
)SQL") != 0)
        {
            throw WorldDatabaseException(
                "Every initial WorldPlayerSeason must have exactly one active roster assignment.");
        }
    }

    std::filesystem::path prepareCoreSnapshot(
        const WorldDatabaseCreateRequest& request,
        const std::filesystem::path& worldFolder,
        std::string& snapshotHash,
        bool& createdSnapshot)
    {
        const std::filesystem::path snapshotDirectory =
            worldFolder / "CoreSnapshots";

        std::filesystem::create_directories(snapshotDirectory);

        std::filesystem::path temporary =
            snapshotDirectory / ".core_snapshot.building";

        {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
        }

        std::error_code copyError;
        std::filesystem::copy_file(
            request.coreDatabase,
            temporary,
            std::filesystem::copy_options::overwrite_existing,
            copyError);

        if (copyError)
        {
            throw WorldDatabaseException(
                "Unable to copy Core snapshot: " + copyError.message());
        }

        snapshotHash = sha256File(temporary);
        const std::filesystem::path finalSnapshot =
            snapshotDirectory / (snapshotHash + ".db");

        if (std::filesystem::exists(finalSnapshot))
        {
            const std::string existingHash = sha256File(finalSnapshot);
            if (existingHash != snapshotHash)
            {
                std::filesystem::remove(temporary);
                throw WorldDatabaseException(
                    "Existing Core snapshot filename/hash mismatch: " +
                    pathUtf8(finalSnapshot));
            }

            std::filesystem::remove(temporary);
            createdSnapshot = false;
            return finalSnapshot;
        }

        std::error_code renameError;
        std::filesystem::rename(temporary, finalSnapshot, renameError);
        if (renameError)
        {
            std::filesystem::remove(temporary);
            throw WorldDatabaseException(
                "Unable to publish Core snapshot: " + renameError.message());
        }

        createdSnapshot = true;
        return finalSnapshot;
    }

    std::filesystem::path storedSnapshotPath(
        const std::filesystem::path& worldFolder,
        const std::filesystem::path& snapshotPath)
    {
        std::filesystem::path relative =
            snapshotPath.lexically_relative(worldFolder);

        if (relative.empty())
        {
            return snapshotPath.filename();
        }

        return relative;
    }

    void publishWorldDatabase(
        const std::filesystem::path& temporary,
        const std::filesystem::path& finalPath,
        bool overwriteExisting)
    {
        if (!std::filesystem::exists(finalPath))
        {
            std::error_code ec;
            std::filesystem::rename(temporary, finalPath, ec);
            if (ec)
            {
                throw WorldDatabaseException(
                    "Unable to publish World database: " + ec.message());
            }
            return;
        }

        if (!overwriteExisting)
        {
            throw WorldDatabaseException(
                "World database already exists and overwriteExisting is false: " +
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
            throw WorldDatabaseException(
                "Unable to move existing World database aside: " + ec.message());
        }

        std::filesystem::rename(temporary, finalPath, ec);
        if (ec)
        {
            std::error_code restoreError;
            std::filesystem::rename(backup, finalPath, restoreError);

            throw WorldDatabaseException(
                "Unable to publish newly created World database: " + ec.message());
        }

        std::filesystem::remove(backup, ec);
    }

    std::filesystem::path resolveSnapshotPath(
        const std::filesystem::path& worldDatabase,
        const std::filesystem::path& storedPath)
    {
        if (storedPath.is_absolute())
        {
            return storedPath;
        }

        std::filesystem::path folder = worldDatabase.parent_path();
        if (folder.empty())
        {
            folder = ".";
        }

        return folder / storedPath;
    }

    void verifySnapshot(
        const std::filesystem::path& worldDatabase,
        const std::filesystem::path& storedPath,
        const std::string& algorithm,
        const std::string& expectedHash)
    {
        const std::filesystem::path actualPath =
            resolveSnapshotPath(worldDatabase, storedPath);

        if (!std::filesystem::is_regular_file(actualPath))
        {
            throw WorldDatabaseException(
                "World Core snapshot is missing: " + pathUtf8(actualPath));
        }

        if (algorithm != "SHA256" && algorithm != "SHA-256")
        {
            throw WorldDatabaseException(
                "Unsupported Core snapshot hash algorithm: " + algorithm);
        }

        const std::string actualHash = sha256File(actualPath);
        if (actualHash != expectedHash)
        {
            throw WorldDatabaseException(
                "World Core snapshot hash mismatch: " + pathUtf8(actualPath));
        }
    }

    void addDiagnostic(
        WorldDatabaseCreateResult& result,
        DatabaseDiagnosticSeverity severity,
        std::string message,
        const std::filesystem::path& source = {})
    {
        DatabaseDiagnostic diagnostic;
        diagnostic.severity = severity;
        diagnostic.message = std::move(message);
        diagnostic.sourceFile = source;
        diagnostic.sourceLine = 0;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

// -----------------------------------------------------------------------------
// World creation
// -----------------------------------------------------------------------------

WorldDatabaseCreateResult createWorldDatabase(
    const WorldDatabaseCreateRequest& request)
{
    WorldDatabaseCreateResult result;
    result.databasePath = request.outputDatabase;

    std::filesystem::path temporaryWorld;
    std::filesystem::path snapshotPath;
    bool snapshotCreatedByAttempt = false;

    try
    {
        if (request.coreDatabase.empty())
        {
            throw WorldDatabaseException("coreDatabase is required.");
        }

        if (request.outputDatabase.empty())
        {
            throw WorldDatabaseException("outputDatabase is required.");
        }

        if (request.worldName.empty())
        {
            throw WorldDatabaseException("worldName is required.");
        }

        if (request.season <= 0)
        {
            throw WorldDatabaseException("season must be greater than zero.");
        }

        if (!std::filesystem::is_regular_file(request.coreDatabase))
        {
            throw WorldDatabaseException(
                "Core database does not exist: " + pathUtf8(request.coreDatabase));
        }

        if (std::filesystem::exists(request.outputDatabase) &&
            !request.overwriteExisting)
        {
            throw WorldDatabaseException(
                "World database already exists and overwriteExisting is false: " +
                pathUtf8(request.outputDatabase));
        }

        std::filesystem::path worldFolder = request.outputDatabase.parent_path();
        if (worldFolder.empty())
        {
            worldFolder = ".";
        }
        std::filesystem::create_directories(worldFolder);

        // Validate the global Core before copying it. The World is then built
        // entirely from the preserved snapshot, not from this global file.
        std::string sourceRuleset;
        std::string sourceScheduleClockZoneId;

        {
            Database sourceCore;
            if (!sourceCore.open(request.coreDatabase, DatabaseOpenMode::ReadOnly))
            {
                throw WorldDatabaseException(
                    "Unable to open Core database: " + sourceCore.lastError());
            }

            sqlite3* core = WorldDatabaseAccess::handle(sourceCore);
            validateSqliteDatabase(core, "Core database");

            const std::string schemaVersion =
                readCoreMetadata(core, "CoreSchemaVersion");
            const std::string ruleset = readCoreMetadata(core, "Ruleset");
            const std::string seasonText = readCoreMetadata(core, "Season");
            const std::string scheduleClockZoneId =
                readCoreMetadata(core, "ScheduleClockZoneId");

            if ((schemaVersion != "2" && schemaVersion != "3") || readCoreMetadata(core, "TeamDataContractVersion") != "1")
            {
                throw WorldDatabaseException(
                    "UNSUPPORTED_SCHEMA: Core schema '" + schemaVersion + "'. Rebuild a fresh compatible Core; existing files are never migrated.");
            }
            if(schemaVersion=="3" && readCoreMetadata(core,"ClockSchedulerStorageVersion")!="1")
                throw WorldDatabaseException("UNSUPPORTED_SCHEMA: Core3 clock/scheduler storage contract is unavailable");

            if (ruleset.empty())
            {
                throw WorldDatabaseException("Core metadata is missing Ruleset.");
            }

            if (seasonText.empty())
            {
                throw WorldDatabaseException("Core metadata is missing Season.");
            }

            if (parseRequiredInt(seasonText, "Season") != request.season)
            {
                throw WorldDatabaseException(
                    "Requested World season does not match Core metadata Season.");
            }

            if (scheduleClockZoneId.empty())
            {
                throw WorldDatabaseException(
                    "Core metadata is missing ScheduleClockZoneId. Rebuild the "
                    "Core database with the current package builder.");
            }

            // Validate that this zone is understood before doing any file work.
            (void)utcOffsetMinutes(
                scheduleClockZoneId,
                parseLocalDateTime(
                    std::to_string(request.season) + "-07-01",
                    "12:00PM"));

            sourceRuleset = ruleset;
            sourceScheduleClockZoneId = scheduleClockZoneId;
        }

        std::string snapshotHash;
        snapshotPath = prepareCoreSnapshot(
            request,
            worldFolder,
            snapshotHash,
            snapshotCreatedByAttempt);

        result.coreSnapshotPath = snapshotPath;
        result.coreSnapshotFileHash = snapshotHash;

        Database coreSnapshot;
        if (!coreSnapshot.open(snapshotPath, DatabaseOpenMode::ReadOnly))
        {
            throw WorldDatabaseException(
                "Unable to open preserved Core snapshot: " +
                coreSnapshot.lastError());
        }

        sqlite3* core = WorldDatabaseAccess::handle(coreSnapshot);
        validateSqliteDatabase(core, "Preserved Core snapshot");

        // Re-read metadata from the preserved snapshot; this is now the source
        // of truth used to instantiate the World.
        const std::string ruleset = readCoreMetadata(core, "Ruleset");
        const std::string scheduleClockZoneId =
            readCoreMetadata(core, "ScheduleClockZoneId");

        if (ruleset != sourceRuleset ||
            scheduleClockZoneId != sourceScheduleClockZoneId)
        {
            throw WorldDatabaseException(
                "Preserved Core snapshot metadata differs from the validated source Core.");
        }

        const CoreCompetitionSelection competition =
            selectCoreCompetition(core, request.season);

        const std::vector<CoreScheduleRow> schedule =
            loadCoreRegularScheduleRows(
                core,
                competition.coreCompetitionSeasonId,
                scheduleClockZoneId);

        const std::string worldStartUtc =
            initialWorldTimeUtc(schedule, scheduleClockZoneId);
        const std::int32_t firstWeek = initialWeek(schedule);

        const std::string createdUtc = nowUtc();
        const std::string worldKey =
            request.worldKey.empty()
            ? generateWorldKey(request, createdUtc)
            : request.worldKey;

        temporaryWorld = request.outputDatabase;
        temporaryWorld += ".building";

        {
            std::error_code ec;
            std::filesystem::remove(temporaryWorld, ec);
        }

        Database worldDatabase;
        if (!WorldDatabaseAccess::openForBuild(worldDatabase, temporaryWorld))
        {
            throw WorldDatabaseException(
                "Unable to create temporary World database: " +
                worldDatabase.lastError());
        }

        sqlite3* world = WorldDatabaseAccess::handle(worldDatabase);

        exec(world, "PRAGMA foreign_keys = ON;");
        exec(world, "PRAGMA journal_mode = DELETE;");
        exec(world, "PRAGMA synchronous = NORMAL;");
        exec(world, "BEGIN IMMEDIATE TRANSACTION;");

        const std::filesystem::path relativeSnapshot =
            storedSnapshotPath(worldFolder, snapshotPath);

        // Keep all prepared statements inside the transaction scope.
        //
        // This is especially important on Windows. Database::close() uses
        // sqlite3_close_v2(), which defers the real SQLite connection close
        // while prepared statements still exist. If we try to rename
        // World.db.building before those Statement destructors have finalized
        // their sqlite3_stmt handles, Windows correctly reports that the file
        // is still in use by this process.
        try
        {
            createWorldSchema(world);
            const auto clockSchema=clockSchedulerWorldSchema();
            exec(world, clockSchema.c_str());

            Statement schemaInsert(
                world,
                R"SQL(
INSERT INTO WorldSchemaInfo
(
    SchemaInfoID, SchemaVersion, SchemaFamily,
    CreatedByEngineBuild, LastMigratedByBuild,
    CreatedUtc, LastMigratedUtc
)
VALUES(1, ?, 'GCE_WORLD', 'CLOCK-SCHEDULER-STAGE1-2026-11-04', NULL, ?, NULL);
)SQL");
            bindInt(schemaInsert.get(), 1, WorldDatabaseSchemaVersion);
            bindText(schemaInsert.get(), 2, createdUtc);
            schemaInsert.stepDone(world);

            Statement worldInsert(
                world,
                R"SQL(
INSERT INTO Worlds
(
    WorldID, WorldKey, WorldName, CreatedUtc, WorldStatus, CurrentTimeUtc
)
VALUES(1, ?, ?, ?, 'ACTIVE', ?);
)SQL");
            bindText(worldInsert.get(), 1, worldKey);
            bindText(worldInsert.get(), 2, request.worldName);
            bindText(worldInsert.get(), 3, createdUtc);
            bindText(worldInsert.get(), 4, worldStartUtc);
            worldInsert.stepDone(world);

            Statement snapshotInsert(
                world,
                R"SQL(
INSERT INTO WorldCoreSnapshots
(
    WorldID,
    CorePackageKey,
    CoreContentHash,
    SourceManifestHash,
    SimulationRulesProfileKey,
    SeasonPackageKey,
    SnapshotPath,
    SnapshotHashAlgorithm,
    SnapshotFileHash,
    AddedUtc
)
VALUES(1, NULL, NULL, NULL, ?, NULL, ?, 'SHA256', ?, ?);
)SQL");
            bindText(snapshotInsert.get(), 1, ruleset);
            bindText(snapshotInsert.get(), 2, pathUtf8(relativeSnapshot));
            bindText(snapshotInsert.get(), 3, snapshotHash);
            bindText(snapshotInsert.get(), 4, createdUtc);
            snapshotInsert.stepDone(world);

            CreationContext context;
            context.core = core;
            context.world = world;
            context.result = &result;
            context.worldCoreSnapshotId = lastInsertId(world);

            insertWorldCompetitionAndSeason(
                context,
                competition,
                ruleset,
                scheduleClockZoneId,
                request.season);

            instantiateGroups(context, competition.coreCompetitionSeasonId);
            instantiateVenues(context);
            instantiateTeams(context, competition.coreCompetitionSeasonId);
            instantiateCorePeople(context);
            instantiateTeamData(context, competition.coreCompetitionSeasonId, worldStartUtc);
            instantiatePlayers(
                context,
                competition.coreCompetitionSeasonId,
                worldStartUtc,
                firstWeek);
            instantiateSchedule(context, schedule);
            instantiateLeagueWeeks(context, worldStartUtc);
            instantiateKickoffEvents(context, worldStartUtc);
            instantiateClockScheduler(context,competition.coreCompetitionSeasonId,worldStartUtc,request);
            if (!worldEventContractsReady(world, context.worldId) || !worldEventPublicationReady(world, context.worldId))
                throw WorldDatabaseException("INTEGRITY_FAILURE: incomplete dated-event construction");

            validateCreationParity(context);
            validateSqliteDatabase(world, "World database");

            exec(world, "COMMIT;");
        }
        catch (...)
        {
            // Any Statement objects created above are destroyed/finalized while
            // unwinding the try block, before control reaches this handler.
            try
            {
                exec(world, "ROLLBACK;");
            }
            catch (...)
            {
                // Preserve the original exception.
            }

            worldDatabase.close();
            coreSnapshot.close();
            throw;
        }

        // The transaction scope has ended, so every prepared Statement above
        // has now been finalized. Closing the Database can therefore release
        // the Windows file handle immediately before publication.
        worldDatabase.close();
        coreSnapshot.close();

        // Verify the snapshot once more immediately before publication.
        verifySnapshot(
            request.outputDatabase,
            relativeSnapshot,
            "SHA256",
            snapshotHash);

        publishWorldDatabase(
            temporaryWorld,
            request.outputDatabase,
            request.overwriteExisting);

        result.success = true;
        result.databasePath = request.outputDatabase;
        result.worldId = 1;
        result.worldKey = worldKey;

        addDiagnostic(
            result,
            DatabaseDiagnosticSeverity::Info,
            "World database created successfully.",
            request.outputDatabase);

        addDiagnostic(
            result,
            DatabaseDiagnosticSeverity::Info,
            "Preserved Core snapshot SHA-256: " + snapshotHash,
            snapshotPath);
    }
    catch (const std::exception& ex)
    {
        addDiagnostic(
            result,
            DatabaseDiagnosticSeverity::Error,
            ex.what());

        if (!temporaryWorld.empty())
        {
            std::error_code ec;
            std::filesystem::remove(temporaryWorld, ec);
        }

        if (snapshotCreatedByAttempt && !snapshotPath.empty())
        {
            std::error_code ec;
            std::filesystem::remove(snapshotPath, ec);
        }

        result.success = false;
    }

    return result;
}

// -----------------------------------------------------------------------------
// World loading / identity
// -----------------------------------------------------------------------------

WorldDatabaseLoadResult loadWorldDatabase(
    const std::filesystem::path& databasePath)
{
    WorldDatabaseLoadResult result;

    Database database;
    if (!database.open(databasePath, DatabaseOpenMode::ReadOnly))
    {
        result.errorMessage = database.lastError();
        return result;
    }

    try
    {
        sqlite3* db = WorldDatabaseAccess::handle(database);
        exec(db, "BEGIN"); // One consistent read snapshot for cross-row readiness.
        validateSqliteDatabase(db, "World database");

        {
            Statement statement(
                db,
                R"SQL(
SELECT SchemaVersion, SchemaFamily
FROM WorldSchemaInfo
WHERE SchemaInfoID = 1;
)SQL");

            if (!statement.stepRow(db))
            {
                throw WorldDatabaseException("WorldSchemaInfo row is missing.");
            }

            result.schemaVersion = sqlite3_column_int(statement.get(), 0);
            const std::string family = columnText(statement.get(), 1);

            if (family != "GCE_WORLD")
            {
                throw WorldDatabaseException(
                    "Unsupported World schema family: " + family);
            }

            if (result.schemaVersion != 4 && result.schemaVersion != WorldDatabaseSchemaVersion)
            {
                throw WorldDatabaseException(
                    "UNSUPPORTED_SCHEMA: create a fresh compatible World; existing saves are not migrated. Found version " +
                    std::to_string(result.schemaVersion));
            }
        }

        if (scalarInt64(db, "SELECT COUNT(*) FROM Worlds;") != 1)
        {
            throw WorldDatabaseException(
                "World.db must contain exactly one World row.");
        }

        {
            Statement statement(
                db,
                R"SQL(
SELECT WorldID, WorldKey, WorldName, WorldStatus, CreatedUtc, CurrentTimeUtc
FROM Worlds;
)SQL");

            statement.stepRow(db);
            result.world.worldId = sqlite3_column_int64(statement.get(), 0);
            result.world.worldKey = columnText(statement.get(), 1);
            result.world.worldName = columnText(statement.get(), 2);
            result.world.worldStatus = columnText(statement.get(), 3);
            result.world.createdUtc = columnText(statement.get(), 4);
            result.world.currentTimeUtc = columnText(statement.get(), 5);

            if (result.world.worldId != 1)
            {
                throw WorldDatabaseException(
                    "World.db single-World invariant requires WorldID = 1.");
            }
        }

        if (TeamDataSql::scalar(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='WorldFeatureVersions'") != 1 ||
            !worldEventContractsReady(db, 1))
            throw WorldDatabaseException("UNSUPPORTED_SCHEMA: unsupported venue/week feature contracts; create a fresh compatible World.");
        if (!worldEventPublicationReady(db, 1))
            throw WorldDatabaseException("INTEGRITY_FAILURE: incomplete dated-event publication");
        result.world.dataRevision = TeamDataSql::scalar(db,"SELECT Revision FROM WorldDataRevisions WHERE WorldID=1");

        {
            Statement statement(
                db,
                R"SQL(
SELECT
    WorldCoreSnapshotID,
    COALESCE(CorePackageKey, ''),
    COALESCE(CoreContentHash, ''),
    COALESCE(SourceManifestHash, ''),
    SimulationRulesProfileKey,
    COALESCE(SeasonPackageKey, ''),
    SnapshotPath,
    SnapshotHashAlgorithm,
    SnapshotFileHash,
    AddedUtc
FROM WorldCoreSnapshots
WHERE WorldID = 1
ORDER BY WorldCoreSnapshotID;
)SQL");

            while (statement.stepRow(db))
            {
                WorldCoreSnapshotRecord snapshot;
                snapshot.worldCoreSnapshotId =
                    sqlite3_column_int64(statement.get(), 0);
                snapshot.corePackageKey = columnText(statement.get(), 1);
                snapshot.coreContentHash = columnText(statement.get(), 2);
                snapshot.sourceManifestHash = columnText(statement.get(), 3);
                snapshot.simulationRulesProfileKey = columnText(statement.get(), 4);
                snapshot.seasonPackageKey = columnText(statement.get(), 5);
                snapshot.snapshotPath = columnText(statement.get(), 6);
                const std::string algorithm = columnText(statement.get(), 7);
                snapshot.snapshotFileHash = columnText(statement.get(), 8);
                snapshot.addedUtc = columnText(statement.get(), 9);

                verifySnapshot(
                    databasePath,
                    snapshot.snapshotPath,
                    algorithm,
                    snapshot.snapshotFileHash);

                result.coreSnapshots.push_back(std::move(snapshot));
            }
        }

        if (result.coreSnapshots.empty())
        {
            throw WorldDatabaseException(
                "World contains no preserved Core snapshot records.");
        }

        {
            Statement statement(
                db,
                R"SQL(
SELECT
    wc.WorldCompetitionID,
    cs.CompetitionSeasonID,
    COALESCE(cs.WorldCoreSnapshotID, 0),
    wc.CompetitionKey,
    wc.Name,
    wc.CompetitionType,
    COALESCE(wc.LevelCode, ''),
    cs.SeasonKey,
    cs.SeasonLabel,
    cs.SeasonYear,
    cs.Status,
    COALESCE(cs.CurrentPhaseCode, ''),
    COALESCE((
        SELECT MIN(sg.WeekNumber)
        FROM WorldScheduledGames sg
        WHERE sg.CompetitionSeasonID = cs.CompetitionSeasonID
          AND sg.StageCode = 'REGULAR'
          AND sg.WeekNumber IS NOT NULL
          AND sg.Status NOT IN ('CANCELLED', 'VOID')
          AND NOT EXISTS (SELECT 1 FROM Games final JOIN Worlds clock ON clock.WorldID=final.WorldID
           WHERE final.WorldScheduledGameID=sg.WorldScheduledGameID AND final.GameStatus='FINAL' AND final.CompletedUtc<=clock.CurrentTimeUtc)
    ), 0)
FROM CompetitionSeasons cs
JOIN WorldCompetitions wc
    ON wc.WorldCompetitionID = cs.WorldCompetitionID
WHERE wc.WorldID = 1
ORDER BY cs.SeasonYear, wc.CompetitionKey;
)SQL");

            while (statement.stepRow(db))
            {
                WorldCompetitionSeasonRecord season;
                season.worldCompetitionId =
                    sqlite3_column_int64(statement.get(), 0);
                season.competitionSeasonId =
                    sqlite3_column_int64(statement.get(), 1);
                season.worldCoreSnapshotId =
                    sqlite3_column_int64(statement.get(), 2);
                season.competitionKey = columnText(statement.get(), 3);
                season.competitionName = columnText(statement.get(), 4);
                season.competitionType = columnText(statement.get(), 5);
                season.levelCode = columnText(statement.get(), 6);
                season.seasonKey = columnText(statement.get(), 7);
                season.seasonLabel = columnText(statement.get(), 8);
                season.seasonYear = sqlite3_column_int(statement.get(), 9);
                season.status = columnText(statement.get(), 10);
                season.currentPhaseCode = columnText(statement.get(), 11);
                season.currentWeek = sqlite3_column_int(statement.get(), 12);
                result.competitionSeasons.push_back(std::move(season));
            }
        }

        if (result.competitionSeasons.empty())
        {
            throw WorldDatabaseException(
                "World contains no CompetitionSeason records.");
        }

        exec(db, "COMMIT");
        result.success = true;
    }
    catch (const std::exception& ex)
    {
        result.errorMessage = ex.what();
        result.coreSnapshots.clear();
        result.competitionSeasons.clear();
        result.success = false;
    }

    return result;
}

// -----------------------------------------------------------------------------
// World team read API
// -----------------------------------------------------------------------------

WorldTeamsLoadResult loadWorldTeams(
    const std::filesystem::path& databasePath,
    std::int64_t competitionSeasonId)
{
    WorldTeamsLoadResult result;

    Database database;
    if (!database.open(databasePath, DatabaseOpenMode::ReadOnly))
    {
        result.errorMessage = database.lastError();
        return result;
    }

    try
    {
        sqlite3* db = WorldDatabaseAccess::handle(database);

        Statement statement(
            db,
            R"SQL(
SELECT
    wo.WorldOrganizationID,
    ts.TeamSeasonID,
    wo.OrganizationKey,
    COALESCE(ts.CoreTeamKey, ''),
    ts.DisplayName,
    COALESCE(ts.MarketName, ''),
    COALESCE(ts.Nickname, ''),
    COALESCE(ts.Abbreviation, ''),
    COALESCE(MAX(CASE
        WHEN UPPER(wg.GroupType) = 'CONFERENCE' THEN wg.GroupName
    END), ''),
    COALESCE(MAX(CASE
        WHEN UPPER(wg.GroupType) = 'DIVISION' THEN wg.GroupName
    END), '')
FROM TeamSeasons ts
JOIN WorldOrganizations wo
    ON wo.WorldOrganizationID = ts.WorldOrganizationID
LEFT JOIN TeamGroupMemberships tgm
    ON tgm.TeamSeasonID = ts.TeamSeasonID
LEFT JOIN WorldCompetitionGroups wg
    ON wg.WorldGroupID = tgm.WorldGroupID
WHERE ts.CompetitionSeasonID = ?
GROUP BY
    wo.WorldOrganizationID,
    ts.TeamSeasonID,
    wo.OrganizationKey,
    ts.CoreTeamKey,
    ts.DisplayName,
    ts.MarketName,
    ts.Nickname,
    ts.Abbreviation
ORDER BY
    9,
    10,
    ts.DisplayName,
    wo.OrganizationKey;
)SQL");

        bindInt64(statement.get(), 1, competitionSeasonId);

        while (statement.stepRow(db))
        {
            WorldTeamRecord team;
            team.worldOrganizationId =
                sqlite3_column_int64(statement.get(), 0);
            team.teamSeasonId = sqlite3_column_int64(statement.get(), 1);
            team.organizationKey = columnText(statement.get(), 2);
            team.coreTeamKey = columnText(statement.get(), 3);
            team.displayName = columnText(statement.get(), 4);
            team.marketName = columnText(statement.get(), 5);
            team.nickname = columnText(statement.get(), 6);
            team.abbreviation = columnText(statement.get(), 7);
            team.conference = columnText(statement.get(), 8);
            team.division = columnText(statement.get(), 9);
            result.teams.push_back(std::move(team));
        }

        if (result.teams.empty())
        {
            result.errorMessage =
                "No World teams were found for the requested CompetitionSeason.";
            return result;
        }

        result.success = true;
    }
    catch (const std::exception& ex)
    {
        result.errorMessage = ex.what();
        result.teams.clear();
        result.success = false;
    }

    return result;
}

// -----------------------------------------------------------------------------
// World schedule read API
// -----------------------------------------------------------------------------

WorldScheduleLoadResult loadWorldCompetitionSchedule(
    const std::filesystem::path& databasePath,
    std::int64_t competitionSeasonId)
{
    WorldScheduleLoadResult result;

    Database database;
    if (!database.open(databasePath, DatabaseOpenMode::ReadOnly))
    {
        result.errorMessage = database.lastError();
        return result;
    }

    try
    {
        sqlite3* db = WorldDatabaseAccess::handle(database);

        std::string informationHorizon;
        {
            Statement worldTime(
                db,
                "SELECT CurrentTimeUtc FROM Worlds WHERE WorldID = 1;");

            if (!worldTime.stepRow(db))
            {
                throw WorldDatabaseException("World row is missing.");
            }

            informationHorizon = columnText(worldTime.get(), 0);
        }

        const bool clockModel=ClockStateSql::enabled(db);
        const std::string venueTable=clockModel?"WorldVenueProjection":"WorldVenues";
        std::string scheduleSql=R"SQL(
SELECT
    sg.WorldScheduledGameID,
    sg.CompetitionSeasonID,
    COALESCE(g.GameID, 0),
    COALESCE(sg.OriginCoreGameKey, ''),
    sg.StageCode,
    COALESCE(sg.WeekNumber, 0),
    COALESCE(sg.GameDateLabel, ''),
    COALESCE(sg.DayLabel, ''),
    COALESCE(sg.TimeLabel, ''),
    sg.SiteCode,
    COALESCE(sg.ScheduledUtc, ''),
    sg.AwayTeamSeasonID,
    sg.HomeTeamSeasonID,
    away.DisplayName,
    home.DisplayName,
    sg.Status,
    COALESCE(g.GameStatus, ''),
    COALESCE(g.StartedUtc, ''),
    COALESCE(g.CompletedUtc, ''),
    g.AwayScore,
    g.HomeScore,
    COALESCE(away.Nickname, ''),
    COALESCE(home.Nickname, ''),
    COALESCE(CASE WHEN evidence.GameID IS NOT NULL AND evidence.CapturedUtc <= (SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=1) THEN evidence.VenueName ELSE venue.VenueName END, ''),
    CASE WHEN g.SimulationProviderKey='PFF_GENERIC_QUEUE_TEST' AND EXISTS(SELECT 1 FROM GameArtifacts a WHERE a.GameID=g.GameID AND a.ArtifactType='TEAM_STATISTICS') THEN 1 ELSE 0 END
FROM WorldScheduledGames sg
JOIN TeamSeasons away
    ON away.TeamSeasonID = sg.AwayTeamSeasonID
JOIN TeamSeasons home
    ON home.TeamSeasonID = sg.HomeTeamSeasonID
LEFT JOIN Games g
    ON g.WorldScheduledGameID = sg.WorldScheduledGameID
   AND g.GameStatus <> 'VOID'
LEFT JOIN GameVenueEvidence evidence ON evidence.GameID=g.GameID AND evidence.WorldID=g.WorldID
LEFT JOIN WorldVenues venue
    ON venue.WorldVenueID = sg.WorldVenueID
WHERE sg.CompetitionSeasonID = ?
ORDER BY
    sg.ScheduledUtc,
    sg.ScheduleKey;
)SQL";
        const auto venueJoin=scheduleSql.find("LEFT JOIN WorldVenues venue");
        scheduleSql.replace(venueJoin,std::string("LEFT JOIN WorldVenues venue").size(),"LEFT JOIN "+venueTable+" venue");
        Statement statement(db,scheduleSql.c_str());

        bindInt64(statement.get(), 1, competitionSeasonId);

        std::set<std::int32_t> regularWeeks;

        while (statement.stepRow(db))
        {
            WorldScheduleGameRecord game;
            game.scheduledGameId = sqlite3_column_int64(statement.get(), 0);
            game.competitionSeasonId = sqlite3_column_int64(statement.get(), 1);
            game.gameId = sqlite3_column_int64(statement.get(), 2);
            game.sourceGameId = columnText(statement.get(), 3);
            game.stage = columnText(statement.get(), 4);
            game.weekNumber = sqlite3_column_int(statement.get(), 5);
            game.gameDate = columnText(statement.get(), 6);
            game.dayLabel = columnText(statement.get(), 7);
            game.timeLabel = columnText(statement.get(), 8);
            game.site = columnText(statement.get(), 9);
            game.scheduledKickoffUtc = columnText(statement.get(), 10);
            game.awayTeamSeasonId = sqlite3_column_int64(statement.get(), 11);
            game.homeTeamSeasonId = sqlite3_column_int64(statement.get(), 12);
            game.awayDisplayName = columnText(statement.get(), 13);
            game.homeDisplayName = columnText(statement.get(), 14);
            game.awayNickname = columnText(statement.get(), 21);
            game.homeNickname = columnText(statement.get(), 22);
            game.venueName = columnText(statement.get(), 23);

            const std::string scheduleStatus = columnText(statement.get(), 15);
            const std::string gameStatus = columnText(statement.get(), 16);
            const std::string startedUtc = columnText(statement.get(), 17);
            const std::string completedUtc = columnText(statement.get(), 18);

            const bool startedVisible =
                !startedUtc.empty() && startedUtc <= informationHorizon;
            const bool completedVisible =
                (!completedUtc.empty() && completedUtc <= informationHorizon) || sqlite3_column_int(statement.get(),24)==1;

            game.status = scheduleStatus;

            if (!gameStatus.empty())
            {
                if (gameStatus == "FINAL")
                {
                    if (completedVisible)
                    {
                        game.status = "FINAL";

                        if (sqlite3_column_type(statement.get(), 19) != SQLITE_NULL &&
                            sqlite3_column_type(statement.get(), 20) != SQLITE_NULL)
                        {
                            game.hasFinalScore = true;
                            game.hasVisibleScore = true;
                            game.awayScore = sqlite3_column_int(statement.get(), 19);
                            game.homeScore = sqlite3_column_int(statement.get(), 20);
                        }
                    }
                    else
                    {
                        game.status = startedVisible ? "IN_PROGRESS" : "SCHEDULED";
                    }
                }
                else if (gameStatus == "IN_PROGRESS")
                {
                    game.status = startedVisible ? "IN_PROGRESS" : "SCHEDULED";
                    if (startedVisible &&
                        sqlite3_column_type(statement.get(), 19) != SQLITE_NULL &&
                        sqlite3_column_type(statement.get(), 20) != SQLITE_NULL)
                    {
                        game.hasVisibleScore = true;
                        game.awayScore = sqlite3_column_int(statement.get(), 19);
                        game.homeScore = sqlite3_column_int(statement.get(), 20);
                    }
                }
                else if (!startedVisible && scheduleStatus == "SCHEDULED")
                {
                    game.status = "SCHEDULED";
                }
                else
                {
                    game.status = gameStatus;
                }
            }

            if (game.stage == "REGULAR" && game.weekNumber > 0)
            {
                regularWeeks.insert(game.weekNumber);
            }

            result.games.push_back(std::move(game));
        }

        if (result.games.empty())
        {
            result.errorMessage =
                "No World schedule games were found for the requested CompetitionSeason.";
            return result;
        }

        result.regularSeasonWeeks.assign(
            regularWeeks.begin(),
            regularWeeks.end());

        result.success = true;
    }
    catch (const std::exception& ex)
    {
        result.errorMessage = ex.what();
        result.games.clear();
        result.regularSeasonWeeks.clear();
        result.success = false;
    }

    return result;
}

