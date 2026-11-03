#pragma once
// TEAMDATA-STAGE2-2026-11-03: approved stage-1 construction DDL; fresh databases only.
inline constexpr const char* CoreTeamDataSchema = R"TD_SQL(
-- TEAMDATA-STAGE1-2026-11-03: Core schema 2 construction fragment, design v0.1.
-- Execute only after the unchanged Core baseline DDL in a NEW empty database.
-- Not an upgrade script. Builder writes CoreSchemaVersion=2 after validation.
PRAGMA foreign_keys = ON;
CREATE TABLE CorePeople (
    CorePersonKey TEXT PRIMARY KEY,
    FirstName TEXT,
    LastName TEXT,
    DisplayName TEXT NOT NULL CHECK(length(trim(DisplayName)) > 0),
    BirthDate TEXT,
    IdentityStatus TEXT NOT NULL CHECK(IdentityStatus IN ('EXPLICIT','LEGACY_PLAYER_BRIDGE')),
    SourceFileID INTEGER REFERENCES SourceFiles(SourceFileID),
    SourceLine INTEGER
);
CREATE TABLE CorePlayerPeople (
    PlayerID INTEGER PRIMARY KEY REFERENCES Players(PlayerID),
    CorePersonKey TEXT NOT NULL REFERENCES CorePeople(CorePersonKey)
);
CREATE TABLE CoreStaffRoles (
    RoleKey TEXT PRIMARY KEY,
    DisplayName TEXT NOT NULL CHECK(length(trim(DisplayName)) > 0)
);
CREATE TABLE CoreStaffAssignments (
    CoreStaffAssignmentID INTEGER PRIMARY KEY,
    TeamSeasonID INTEGER NOT NULL REFERENCES TeamSeasons(TeamSeasonID),
    RoleKey TEXT NOT NULL REFERENCES CoreStaffRoles(RoleKey),
    CorePersonKey TEXT NOT NULL REFERENCES CorePeople(CorePersonKey),
    SourceFileID INTEGER REFERENCES SourceFiles(SourceFileID),
    SourceLine INTEGER,
    UNIQUE(TeamSeasonID,RoleKey)
);
CREATE TABLE CoreStaffSourceLabels (
    TeamSeasonID INTEGER NOT NULL REFERENCES TeamSeasons(TeamSeasonID),
    RoleKey TEXT NOT NULL REFERENCES CoreStaffRoles(RoleKey),
    RawLabel TEXT NOT NULL,
    SourceFileID INTEGER NOT NULL REFERENCES SourceFiles(SourceFileID),
    SourceLine INTEGER NOT NULL,
    PRIMARY KEY(TeamSeasonID,RoleKey,SourceFileID,SourceLine)
);
CREATE TABLE CoreLeagueGameWeekPeriods (
    PeriodKey TEXT PRIMARY KEY,
    CompetitionSeasonID INTEGER NOT NULL REFERENCES CompetitionSeasons(CompetitionSeasonID),
    CalendarKey TEXT NOT NULL,
    CalendarPolicyVersion TEXT NOT NULL,
    PhaseCode TEXT NOT NULL CHECK(PhaseCode IN ('PRESEASON','REGULAR','POSTSEASON','OFFSEASON')),
    DisplayWeekNumber INTEGER,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT NOT NULL,
    SourceFileID INTEGER REFERENCES SourceFiles(SourceFileID),
    SourceLine INTEGER,
    CHECK(StartsUtc < EndsUtc),
    UNIQUE(CalendarKey,StartsUtc)
);
CREATE TRIGGER CoreLeagueWeekNoOverlap BEFORE INSERT ON CoreLeagueGameWeekPeriods
WHEN EXISTS (SELECT 1 FROM CoreLeagueGameWeekPeriods p
    WHERE p.CalendarKey=NEW.CalendarKey AND p.StartsUtc<NEW.EndsUtc AND NEW.StartsUtc<p.EndsUtc)
BEGIN SELECT RAISE(ABORT,'game-week period overlap'); END;
CREATE TRIGGER CoreLeagueWeekUpdateNoOverlap BEFORE UPDATE ON CoreLeagueGameWeekPeriods
WHEN EXISTS (SELECT 1 FROM CoreLeagueGameWeekPeriods p
    WHERE p.PeriodKey<>OLD.PeriodKey AND p.CalendarKey=NEW.CalendarKey
      AND p.StartsUtc<NEW.EndsUtc AND NEW.StartsUtc<p.EndsUtc)
BEGIN SELECT RAISE(ABORT,'game-week period overlap'); END;

)TD_SQL";
