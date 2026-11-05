#pragma once
// CLOCK-SCHEDULER-STAGE1-2026-11-04. Fresh Core3 additions, never a migration.
inline constexpr const char* CoreClockSchema=R"CLOCK_SQL(-- CLOCK-SCHEDULER-DESIGN-2026-11-04; adopted construction DDL, never a migration.
-- Fresh Core only: append to the existing Core baseline and retained Core2
-- people/staff extension. Validate before writing CoreSchemaVersion=3.
PRAGMA foreign_keys=ON;
CREATE TABLE CoreGameClockProfiles (
 ProfileKey TEXT NOT NULL, ProfileVersion INTEGER NOT NULL CHECK(ProfileVersion>0),
 RegulationPeriods INTEGER NOT NULL CHECK(RegulationPeriods=4),
 QuarterSeconds INTEGER NOT NULL CHECK(QuarterSeconds>0),
 OvertimePolicyKey TEXT NOT NULL, TimingPolicyKey TEXT NOT NULL,
 TimingPolicyVersion INTEGER NOT NULL CHECK(TimingPolicyVersion>0),
 PolicyContentHash TEXT NOT NULL, SourceFileID INTEGER REFERENCES SourceFiles(SourceFileID), SourceLine INTEGER,
 PRIMARY KEY(ProfileKey,ProfileVersion)
);
CREATE TABLE CoreSeasonEventDefinitions (
 EventKey TEXT PRIMARY KEY, SeasonKey TEXT NOT NULL, EventType TEXT NOT NULL,
 TriggerKind TEXT NOT NULL CHECK(TriggerKind IN ('FIXED','TEAM_HOME_LOCAL','AFTER_CHAMPIONSHIP_FINAL')),
 LocalDateTime TEXT, SourceZone TEXT, FixedUtc TEXT,
 Priority INTEGER NOT NULL CHECK(Priority>=0), HandlerPolicyKey TEXT NOT NULL,
 HandlerPolicyVersion INTEGER NOT NULL CHECK(HandlerPolicyVersion>0),
 ScopeKey TEXT NOT NULL, EligibilityPolicyKey TEXT, MediaOnly INTEGER NOT NULL CHECK(MediaOnly IN (0,1)),
 SourceFileID INTEGER NOT NULL REFERENCES SourceFiles(SourceFileID), SourceLine INTEGER NOT NULL CHECK(SourceLine>0),
 RawSource TEXT NOT NULL,
 CHECK(COALESCE((TriggerKind='FIXED' AND LocalDateTime IS NOT NULL AND SourceZone IS NOT NULL AND FixedUtc IS NOT NULL)
 OR (TriggerKind='TEAM_HOME_LOCAL' AND LocalDateTime IS NOT NULL AND SourceZone IS NULL AND FixedUtc IS NULL)
 OR (TriggerKind='AFTER_CHAMPIONSHIP_FINAL' AND LocalDateTime IS NULL AND SourceZone IS NULL AND FixedUtc IS NULL),0)),
 CHECK(FixedUtc IS NULL OR COALESCE(length(FixedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',FixedUtc)=FixedUtc,0))
);
CREATE TABLE CoreSeasonEventDependencies (
 EventKey TEXT NOT NULL REFERENCES CoreSeasonEventDefinitions(EventKey),
 RequiresEventKey TEXT NOT NULL REFERENCES CoreSeasonEventDefinitions(EventKey),
 PRIMARY KEY(EventKey,RequiresEventKey), CHECK(EventKey<>RequiresEventKey)
);
CREATE TABLE CoreTeamHomeTimeZones (
 TeamSeasonID INTEGER PRIMARY KEY REFERENCES TeamSeasons(TeamSeasonID),
 IanaZoneID TEXT NOT NULL, TzdbVersion TEXT NOT NULL,
 SourceFileID INTEGER NOT NULL REFERENCES SourceFiles(SourceFileID), SourceLine INTEGER NOT NULL CHECK(SourceLine>0)
);
-- Authoritative loader validates named zones, local timestamps, DST ambiguity,
-- dependency acyclicity, supported typed policies and source hashes before commit.
-- No silently invented local zone, quarter length or event handler.
)CLOCK_SQL";
