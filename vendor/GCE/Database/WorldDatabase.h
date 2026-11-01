#pragma once

#include "Database/PFFDatabase.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// World database API
// -----------------------------------------------------------------------------
//
// WorldDatabase is the mutable save-state side of the GCE database model.
//
// CoreDB:
//     immutable compiled content/rules/historical source truth
//
// WorldDB:
//     mutable simulation state created from one or more preserved Core snapshots
//
// Physical-save invariant:
//
//     One World.db file contains exactly one World.
//
// A World may eventually contain many competitions, competition seasons, levels
// of football, organizations, People, and games, but unrelated "parallel
// universes" never share one World.db. They get their own save folders/files.
//
// Recommended GCE save layout:
//
//     Data/World/<user-visible World folder>/
//         World.db
//         CoreSnapshots/
//             <immutable Core snapshot>.db
//         Games/
//             <game/PBP artifacts>
//
// The folder/display name is not authoritative World identity. WorldKey is the
// stable semantic identity stored inside World.db, so a save folder may be
// renamed without creating a new World.
//
// Core snapshot ownership is explicit: World.db records the snapshot path and
// integrity hash. The path says which preserved file this World owns/uses; the
// hash verifies that the preserved snapshot has not been modified afterward.
//
// World stores mutable state and deviations from immutable Core truth. It should
// not duplicate original player-card ratings merely because a World exists.
// Effective runtime values are conceptually:
//
//     preserved Core baseline
//     + World-owned persistent modifiers
//     + World-owned temporary condition/rule modifiers
//
// Examples of World-owned change include injuries, development/practice effects,
// roster movement, season statistics, game results, and later career history.
//
// This first API intentionally exposes only the operations required to create,
// load, inspect, and navigate the initial PFF Classic World. Game-result,
// statistics, injury, postseason, and other mutation APIs should be added only
// with the domain services that own those rules.
//
// This header is intended to remain GCE-facing. PFF Classic controllers may
// supply PFF-specific defaults/configuration, but generic World persistence
// should not grow PFF-only helpers.
//
// WorldDatabase.cpp should reuse the shared Database/SQLite infrastructure rather
// than creating a second independent database abstraction. Shared Database and
// diagnostic types currently live in PFFDatabase.h; that is a transitional
// dependency that can later move into a generic GCE database header.
// -----------------------------------------------------------------------------

inline constexpr std::int32_t WorldDatabaseSchemaVersion = 1;

// -----------------------------------------------------------------------------
// World creation
// -----------------------------------------------------------------------------

struct WorldDatabaseCreateRequest
{
    // Existing compiled Core database used to initialize the World.
    //
    // Initial PFF Classic value:
    //
    //   Data/1989_PFF_Core.db
    //
    // World creation validates the Core and copies it into this World's
    // CoreSnapshots directory before gameplay begins. Normal gameplay never
    // mutates either the supplied global Core or the preserved snapshot.
    std::filesystem::path coreDatabase;

    // Final mutable World database path.
    //
    // Example:
    //
    //   Data/World/My1989World/World.db
    //
    // The World creator derives sibling save directories such as CoreSnapshots
    // from this path; callers should not have to construct snapshot paths.
    std::filesystem::path outputDatabase;

    // Player-facing World name. PFF Classic may present this as the league name
    // in its Commissioner's Office, but the persistence concept remains World.
    std::string worldName;

    // Optional stable semantic World key.
    //
    // If empty, the implementation generates one during creation. UI code
    // should not be responsible for inventing database identity values.
    std::string worldKey;

    // Source season to instantiate from Core. PFF Classic currently supplies
    // 1989 from its product/controller layer; the generic World API does not
    // assume that every GCE World begins in 1989.
    std::int32_t season = 0;

    // False by default so an existing save is never destroyed accidentally.
    bool overwriteExisting = false;
};

struct WorldDatabaseCreateResult
{
    bool success = false;

    std::filesystem::path databasePath;
    std::filesystem::path coreSnapshotPath;

    // Integrity hash of the exact preserved SQLite snapshot file. This is
    // available even before the Core builder gains canonical package/content
    // manifest hashing.
    std::string coreSnapshotFileHash;

    std::int64_t worldId = 0;
    std::string worldKey;

    // Useful creation/parity counts for tests and diagnostics. These describe
    // World records created from Core, not Core records themselves.
    std::size_t competitionsCreated = 0;
    std::size_t competitionSeasonsCreated = 0;
    std::size_t organizationsCreated = 0;
    std::size_t teamSeasonsCreated = 0;
    std::size_t peopleCreated = 0;
    std::size_t worldPlayersCreated = 0;
    std::size_t rosterAssignmentsCreated = 0;
    std::size_t scheduledGamesCreated = 0;

    std::vector<DatabaseDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Creates a new mutable World database from one existing compiled Core.
//
// The initial implementation is intentionally PFF Classic focused:
//
//   1. validate the selected Core database;
//   2. create World.db.building;
//   3. create the v1 World schema;
//   4. copy the selected Core into the World's CoreSnapshots directory;
//   5. hash the exact preserved snapshot and record its path/hash ownership;
//   6. create the single World identity + Core provenance;
//   7. instantiate the selected competition/season structure;
//   8. instantiate organizations + TeamSeasons;
//   9. instantiate People/WorldPlayers/WorldPlayerSeasons + initial rosters;
//  10. instantiate the historical regular-season schedule as World state;
//  11. normalize each source kickoff into canonical World/UTC time;
//  12. run integrity/foreign-key validation;
//  13. publish the completed World only after successful commit.
//
// A failed creation must not replace an existing valid World or leave a partial
// save that appears loadable.
WorldDatabaseCreateResult createWorldDatabase(
    const WorldDatabaseCreateRequest& request);

// -----------------------------------------------------------------------------
// World loading / identity
// -----------------------------------------------------------------------------

struct WorldRecord
{
    std::int64_t worldId = 0;

    std::string worldKey;
    std::string worldName;
    std::string worldStatus;

    std::string createdUtc;

    // Canonical simulation clock for the entire World. Competition seasons do
    // not advance independent clocks. Game eligibility, information visibility,
    // dated injuries/transactions, and later cross-level activity are ordered
    // against this timeline.
    std::string currentTimeUtc;
};

struct WorldCoreSnapshotRecord
{
    std::int64_t worldCoreSnapshotId = 0;

    // Package/content provenance may initially be empty until the Core builder
    // emits canonical package and source-manifest hashes.
    std::string corePackageKey;
    std::string coreContentHash;
    std::string sourceManifestHash;

    std::string simulationRulesProfileKey;
    std::string seasonPackageKey;

    // Path is stored relative to the World save folder where practical.
    // snapshotFileHash verifies the exact immutable SQLite file copied into
    // CoreSnapshots during World creation.
    std::filesystem::path snapshotPath;
    std::string snapshotFileHash;
    std::string addedUtc;
};

struct WorldCompetitionSeasonRecord
{
    std::int64_t worldCompetitionId = 0;
    std::int64_t competitionSeasonId = 0;
    std::int64_t worldCoreSnapshotId = 0;

    std::string competitionKey;
    std::string competitionName;
    std::string competitionType;
    std::string levelCode;

    std::string seasonKey;
    std::string seasonLabel;
    std::int32_t seasonYear = 0;

    std::string status;
    std::string currentPhaseCode;

    // Derived navigation label, never an independently authoritative counter.
    //
    // During the regular season:
    //     currentWeek = earliest week containing any unresolved scheduled game.
    //
    // A later week is not eligible to complete while an earlier week remains
    // unresolved because injuries, roster moves, cuts, trades, and other World
    // state may affect later games. Zero means no regular-season week is active
    // (for example, after the regular season is complete).
    std::int32_t currentWeek = 0;
};

struct WorldDatabaseLoadResult
{
    bool success = false;

    std::int32_t schemaVersion = 0;
    WorldRecord world;
    std::vector<WorldCoreSnapshotRecord> coreSnapshots;
    std::vector<WorldCompetitionSeasonRecord> competitionSeasons;

    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Validates and reads the top-level World identity/configuration needed by the
// application after Load League / New League.
//
// The loader should reject normal read/write use when the schema is unsupported,
// integrity/foreign-key validation fails, World.db does not contain exactly one
// World row, required Core snapshots are missing/hash-invalid, or required
// World/CompetitionSeason relationships cannot be resolved.
WorldDatabaseLoadResult loadWorldDatabase(
    const std::filesystem::path& databasePath);

// -----------------------------------------------------------------------------
// World team read API
// -----------------------------------------------------------------------------

struct WorldTeamRecord
{
    std::int64_t worldOrganizationId = 0;
    std::int64_t teamSeasonId = 0;

    std::string organizationKey;
    std::string coreTeamKey;

    std::string displayName;
    std::string marketName;
    std::string nickname;
    std::string abbreviation;

    std::string conference;
    std::string division;

    [[nodiscard]] std::string fallbackDisplayName() const
    {
        if (!displayName.empty())
        {
            return displayName;
        }

        if (marketName.empty())
        {
            return nickname;
        }

        if (nickname.empty())
        {
            return marketName;
        }

        return marketName + " " + nickname;
    }
};

struct WorldTeamsLoadResult
{
    bool success = false;
    std::vector<WorldTeamRecord> teams;
    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Reads TeamSeason state for one World CompetitionSeason. Team identity and
// group membership come from World state so future realignment/custom Worlds do
// not silently fall back to the global Core structure.
WorldTeamsLoadResult loadWorldTeams(
    const std::filesystem::path& databasePath,
    std::int64_t competitionSeasonId);

// -----------------------------------------------------------------------------
// World schedule read API
// -----------------------------------------------------------------------------

struct WorldScheduleGameRecord
{
    std::int64_t scheduledGameId = 0;
    std::int64_t competitionSeasonId = 0;
    std::int64_t gameId = 0; // 0 until a Game record exists.

    // Historical/Core provenance when this fixture originated from a Core
    // schedule template. Generated/custom games may leave this empty.
    std::string sourceGameId;

    std::string stage;
    std::int32_t weekNumber = 0;

    // Source/display schedule fields are preserved for faithful presentation.
    // For PFF Classic/NFL schedule data, listed kickoff times use one common
    // Eastern league schedule clock rather than each stadium's local clock.
    // Therefore every game listed at the same source kickoff (for example all
    // 1:00 PM Eastern games) starts at the same real-world instant, even though
    // the venue-local wall clock may differ.
    std::string gameDate;
    std::string dayLabel;
    std::string timeLabel;
    std::string site;

    // Canonical absolute kickoff instant used by World orchestration. World
    // creation derives this from the source schedule's stated clock convention.
    // Venue-local time is presentation data and must never change ordering.
    std::string scheduledKickoffUtc;

    std::int64_t awayTeamSeasonId = 0;
    std::int64_t homeTeamSeasonId = 0;

    std::string awayDisplayName;
    std::string homeDisplayName;

    // SCHEDULE-UI-20261101: additive read-model metadata for the owner-requested
    // LeagueScheduleScreen update (2026-11-01). Generic upstream candidate;
    // imported upstream revision unknown. No schema or write-contract change.
    // Implementation and visibility checks: Database/WorldDatabase.cpp;
    // integration fixture: bin/x64/Debug/schedule_validation.cpp in PFF.
    std::string awayNickname;
    std::string homeNickname;
    std::string venueName;

    // Initial values are expected to include SCHEDULED / IN_PROGRESS / FINAL.
    // Keep this as data rather than teaching UI code transition rules.
    std::string status;

    bool hasFinalScore = false;
    // True only for a visible final score or an actually IN_PROGRESS game's
    // available current scoreboard after its start is visible. A future FINAL
    // game presented as IN_PROGRESS does not expose its stored final score.
    bool hasVisibleScore = false;
    std::int32_t awayScore = 0;
    std::int32_t homeScore = 0;

    [[nodiscard]] std::string matchupDisplayName() const
    {
        return awayDisplayName + " at " + homeDisplayName;
    }
};

struct WorldScheduleLoadResult
{
    bool success = false;

    std::vector<std::int32_t> regularSeasonWeeks;
    std::vector<WorldScheduleGameRecord> games;
    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Reads the instantiated schedule for one World CompetitionSeason.
//
// Unlike loadCoreRegularSeasonSchedule(), this returns mutable World fixtures and
// may therefore include game status/results and future generated postseason
// games. Results should be returned in deterministic presentation order using
// canonical scheduled kickoff time, then a stable secondary key.
//
// Games with the same scheduledKickoffUtc form a concurrent kickoff cohort.
// GCE orchestration may advance AI-only games incrementally beside a human game
// for live Broadcast/highlight presentation, or resolve AI-only games headlessly
// at full speed when synchronized presentation is unnecessary.
//
// Information visibility follows World/game time, not database completion order.
// Sequentially resolving two human-controlled games that share a kickoff must
// not let the second game's participants learn future events from the first.
WorldScheduleLoadResult loadWorldCompetitionSchedule(
    const std::filesystem::path& databasePath,
    std::int64_t competitionSeasonId);

// -----------------------------------------------------------------------------
// Intentional v1 boundary
// -----------------------------------------------------------------------------
//
// Do not add generic UI-facing mutation functions such as updateRow(),
// setPlayerStat(), or setStandings().
//
// The next World write APIs should arrive with the domain services that own
// their invariants, for example:
//
//     Game finalization / normalized game history
//     Statistics derivation/cache rebuild
//     Standings derivation/cache rebuild
//     Injury resolution / availability
//     World/week advancement
//     Postseason generation
//
// Those systems should mutate WorldDB transactionally while keeping CoreDB
// immutable and keeping the TUI free of SQL/domain-rule ownership.
//
// Runtime-game boundary:
//
//     Core snapshot + World state + game configuration
//                         |
//                         v
//                    GameSession
//                     (memory)
//
// A live football resolver should operate primarily on an in-memory GameSession,
// not issue SQLite lookups for every snap/decision. Human games may checkpoint
// authoritative play/PBP and World consequences as they occur. AI-vs-AI games
// may resolve an entire game in memory and persist the completed transaction in
// one efficient batch when synchronized presentation is not required.
//
// ScheduledGame and Game remain distinct identities. A fixture exists before it
// is played; GameID is assigned when an actual game execution/history exists.
// Game/PBP artifacts should be linked through GameID/structured artifact metadata
// rather than treating a filename as authoritative game identity.


