#pragma once
// TEAMDATA-STAGE2-2026-11-03: approved stage-1 construction DDL; fresh databases only.
inline constexpr const char* WorldTeamDataSchema = R"TD_SQL(
-- TEAMDATA-STAGE1-2026-11-03: World schema 2 construction fragment, design v0.1.
-- Apply after baseline World DDL only in NEW empty Worlds, never to existing saves.
-- Builder writes WorldSchemaInfo.SchemaVersion=2 after all feature validation.
PRAGMA foreign_keys=ON;
CREATE UNIQUE INDEX TD_OrgWorld ON WorldOrganizations(WorldID,WorldOrganizationID);
CREATE UNIQUE INDEX TD_PersonWorld ON People(WorldID,PersonID);
CREATE UNIQUE INDEX TD_VenueWorld ON WorldVenues(WorldID,WorldVenueID);
CREATE UNIQUE INDEX TD_CompetitionWorld ON WorldCompetitions(WorldID,WorldCompetitionID);
CREATE UNIQUE INDEX TD_SnapshotWorld ON WorldCoreSnapshots(WorldID,WorldCoreSnapshotID);
CREATE UNIQUE INDEX TD_GameWorld ON Games(WorldID,GameID);
CREATE UNIQUE INDEX TD_TeamOrg ON TeamSeasons(WorldOrganizationID,TeamSeasonID);
CREATE TABLE WorldDataRevisions (
    WorldID INTEGER PRIMARY KEY REFERENCES Worlds(WorldID),
    Revision INTEGER NOT NULL DEFAULT 0 CHECK(Revision>=0)
);
CREATE TABLE WorldUsers (
    WorldUserID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
    UserKey TEXT NOT NULL,
    DisplayName TEXT NOT NULL CHECK(length(trim(DisplayName))>0),
    CanManageTeams INTEGER NOT NULL DEFAULT 0 CHECK(CanManageTeams IN (0,1)),
    CanEnableGodMode INTEGER NOT NULL DEFAULT 0 CHECK(CanEnableGodMode IN (0,1)),
    Active INTEGER NOT NULL DEFAULT 1 CHECK(Active IN (0,1)),
    UNIQUE(WorldID,UserKey), UNIQUE(WorldID,WorldUserID)
);
CREATE TABLE WorldUserEditModes (
    WorldID INTEGER NOT NULL,
    WorldUserID INTEGER NOT NULL,
    GodModeEnabled INTEGER NOT NULL DEFAULT 0 CHECK(GodModeEnabled IN (0,1)),
    ChangedUtc TEXT NOT NULL,
    ChangedRevision INTEGER NOT NULL CHECK(ChangedRevision>=0),
    PRIMARY KEY(WorldID,WorldUserID),
    FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID)
);
CREATE TABLE WorldPersonSourceAliases (
    WorldID INTEGER NOT NULL,
    WorldCoreSnapshotID INTEGER NOT NULL,
    CorePersonKey TEXT NOT NULL,
    PersonID INTEGER NOT NULL,
    PRIMARY KEY(WorldID,WorldCoreSnapshotID,CorePersonKey),
    FOREIGN KEY(WorldID,WorldCoreSnapshotID) REFERENCES WorldCoreSnapshots(WorldID,WorldCoreSnapshotID),
    FOREIGN KEY(WorldID,PersonID) REFERENCES People(WorldID,PersonID)
);
CREATE TABLE RoleDefinitions (
    RoleID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
    RoleKey TEXT NOT NULL,
    DisplayName TEXT NOT NULL,
    UNIQUE(WorldID,RoleKey), UNIQUE(WorldID,RoleID)
);
CREATE TABLE AuthorityProfiles (
    AuthorityProfileID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
    ProfileKey TEXT NOT NULL,
    ProfileVersion INTEGER NOT NULL CHECK(ProfileVersion>0),
    UNIQUE(WorldID,ProfileKey,ProfileVersion), UNIQUE(WorldID,AuthorityProfileID)
);
CREATE TABLE AuthorityProfileCapabilities (
    WorldID INTEGER NOT NULL,
    AuthorityProfileID INTEGER NOT NULL,
    CapabilityKey TEXT NOT NULL,
    PRIMARY KEY(WorldID,AuthorityProfileID,CapabilityKey),
)TD_SQL"
R"TD_SQL(
    FOREIGN KEY(WorldID,AuthorityProfileID) REFERENCES AuthorityProfiles(WorldID,AuthorityProfileID)
);
CREATE TABLE AuthorityProfileStaffTargets (
    WorldID INTEGER NOT NULL,
    AuthorityProfileID INTEGER NOT NULL,
    TargetRoleID INTEGER NOT NULL,
    PRIMARY KEY(WorldID,AuthorityProfileID,TargetRoleID),
    FOREIGN KEY(WorldID,AuthorityProfileID) REFERENCES AuthorityProfiles(WorldID,AuthorityProfileID),
    FOREIGN KEY(WorldID,TargetRoleID) REFERENCES RoleDefinitions(WorldID,RoleID)
);
CREATE TABLE RolePositions (
    PositionID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
    ScopeKind TEXT NOT NULL CHECK(ScopeKind IN ('ORGANIZATION','COMPETITION','WORLD')),
    WorldOrganizationID INTEGER,
    WorldCompetitionID INTEGER,
    TeamSeasonID INTEGER,
    PositionKey TEXT NOT NULL,
    RoleID INTEGER NOT NULL,
    AuthorityProfileID INTEGER,
    Active INTEGER NOT NULL DEFAULT 1 CHECK(Active IN (0,1)),
    CHECK((ScopeKind='ORGANIZATION' AND WorldOrganizationID IS NOT NULL AND WorldCompetitionID IS NULL)
       OR (ScopeKind='COMPETITION' AND WorldOrganizationID IS NULL AND WorldCompetitionID IS NOT NULL AND TeamSeasonID IS NULL)
       OR (ScopeKind='WORLD' AND WorldOrganizationID IS NULL AND WorldCompetitionID IS NULL AND TeamSeasonID IS NULL)),
    FOREIGN KEY(WorldID,WorldOrganizationID) REFERENCES WorldOrganizations(WorldID,WorldOrganizationID),
    FOREIGN KEY(WorldID,WorldCompetitionID) REFERENCES WorldCompetitions(WorldID,WorldCompetitionID),
    FOREIGN KEY(WorldOrganizationID,TeamSeasonID) REFERENCES TeamSeasons(WorldOrganizationID,TeamSeasonID),
    FOREIGN KEY(WorldID,RoleID) REFERENCES RoleDefinitions(WorldID,RoleID),
    FOREIGN KEY(WorldID,AuthorityProfileID) REFERENCES AuthorityProfiles(WorldID,AuthorityProfileID),
    UNIQUE(WorldID,PositionID), UNIQUE(WorldID,WorldOrganizationID,PositionID)
);
CREATE UNIQUE INDEX TD_OrgPosition ON RolePositions(WorldID,WorldOrganizationID,PositionKey) WHERE ScopeKind='ORGANIZATION';
CREATE UNIQUE INDEX TD_CompetitionPosition ON RolePositions(WorldID,WorldCompetitionID,PositionKey) WHERE ScopeKind='COMPETITION';
CREATE UNIQUE INDEX TD_WorldPosition ON RolePositions(WorldID,PositionKey) WHERE ScopeKind='WORLD';
CREATE TABLE RoleAssignments (
    RoleAssignmentID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    PositionID INTEGER NOT NULL,
    PersonID INTEGER NOT NULL,
    StartsUtc TEXT NOT NULL,
    StartsRevision INTEGER NOT NULL CHECK(StartsRevision>=0),
    EndsUtc TEXT,
    EndsRevision INTEGER,
    EndReasonCode TEXT,
    CHECK((EndsUtc IS NULL AND EndsRevision IS NULL AND EndReasonCode IS NULL)
       OR (EndsUtc IS NOT NULL AND EndsRevision IS NOT NULL AND EndReasonCode IS NOT NULL
           AND EndsUtc>=StartsUtc AND EndsRevision>=StartsRevision)),
    FOREIGN KEY(WorldID,PositionID) REFERENCES RolePositions(WorldID,PositionID),
    FOREIGN KEY(WorldID,PersonID) REFERENCES People(WorldID,PersonID),
    UNIQUE(WorldID,RoleAssignmentID,PersonID,PositionID)
);
CREATE UNIQUE INDEX TD_OnePositionHolder ON RoleAssignments(PositionID) WHERE EndsUtc IS NULL;
CREATE INDEX TD_ActivePersonRoles ON RoleAssignments(WorldID,PersonID) WHERE EndsUtc IS NULL;
CREATE TABLE RoleCompatibilityPairs (
    WorldID INTEGER NOT NULL,
    FirstRoleID INTEGER NOT NULL,
    SecondRoleID INTEGER NOT NULL,
    CHECK(FirstRoleID<=SecondRoleID),
    PRIMARY KEY(WorldID,FirstRoleID,SecondRoleID),
    FOREIGN KEY(WorldID,FirstRoleID) REFERENCES RoleDefinitions(WorldID,RoleID),
    FOREIGN KEY(WorldID,SecondRoleID) REFERENCES RoleDefinitions(WorldID,RoleID)
);
CREATE TABLE Characters (
    CharacterID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
)TD_SQL"
R"TD_SQL(
    WorldUserID INTEGER NOT NULL,
    PersonID INTEGER NOT NULL,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT,
    CHECK(EndsUtc IS NULL OR EndsUtc>=StartsUtc),
    FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
    FOREIGN KEY(WorldID,PersonID) REFERENCES People(WorldID,PersonID),
    UNIQUE(WorldID,CharacterID,PersonID,WorldUserID)
);
CREATE UNIQUE INDEX TD_OnePersonController ON Characters(WorldID,PersonID) WHERE EndsUtc IS NULL;
CREATE TABLE TeamManagementIntents (
    TeamManagementID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    WorldOrganizationID INTEGER NOT NULL,
    WorldUserID INTEGER NOT NULL,
    CoveragePolicyKey TEXT NOT NULL,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT,
    CHECK(EndsUtc IS NULL OR EndsUtc>=StartsUtc),
    FOREIGN KEY(WorldID,WorldOrganizationID) REFERENCES WorldOrganizations(WorldID,WorldOrganizationID),
    FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
    UNIQUE(WorldID,TeamManagementID,WorldOrganizationID),
    UNIQUE(WorldID,TeamManagementID,WorldUserID)
);
CREATE UNIQUE INDEX TD_OneTeamManager ON TeamManagementIntents(WorldID,WorldOrganizationID) WHERE EndsUtc IS NULL;
CREATE TABLE TeamManagementPositions (
    WorldID INTEGER NOT NULL,
    TeamManagementID INTEGER NOT NULL,
    WorldOrganizationID INTEGER NOT NULL,
    PositionID INTEGER NOT NULL,
    PRIMARY KEY(WorldID,TeamManagementID,PositionID),
    FOREIGN KEY(WorldID,TeamManagementID,WorldOrganizationID)
        REFERENCES TeamManagementIntents(WorldID,TeamManagementID,WorldOrganizationID),
    FOREIGN KEY(WorldID,WorldOrganizationID,PositionID)
        REFERENCES RolePositions(WorldID,WorldOrganizationID,PositionID)
);
CREATE TABLE CharacterRoleControls (
    RoleControlID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    CharacterID INTEGER NOT NULL,
    WorldUserID INTEGER NOT NULL,
    PersonID INTEGER NOT NULL,
    RoleAssignmentID INTEGER NOT NULL,
    PositionID INTEGER NOT NULL,
    OriginKind TEXT NOT NULL CHECK(OriginKind IN ('MANUAL','TEAM_MANAGEMENT')),
    TeamManagementID INTEGER,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT,
    CHECK(EndsUtc IS NULL OR EndsUtc>=StartsUtc),
    CHECK((OriginKind='MANUAL' AND TeamManagementID IS NULL)
       OR (OriginKind='TEAM_MANAGEMENT' AND TeamManagementID IS NOT NULL)),
    FOREIGN KEY(WorldID,CharacterID,PersonID,WorldUserID)
        REFERENCES Characters(WorldID,CharacterID,PersonID,WorldUserID),
    FOREIGN KEY(WorldID,RoleAssignmentID,PersonID,PositionID)
        REFERENCES RoleAssignments(WorldID,RoleAssignmentID,PersonID,PositionID),
    FOREIGN KEY(WorldID,TeamManagementID,WorldUserID)
        REFERENCES TeamManagementIntents(WorldID,TeamManagementID,WorldUserID),
    FOREIGN KEY(WorldID,TeamManagementID,PositionID)
        REFERENCES TeamManagementPositions(WorldID,TeamManagementID,PositionID)
);
CREATE UNIQUE INDEX TD_ManualControl ON CharacterRoleControls(CharacterID,RoleAssignmentID) WHERE OriginKind='MANUAL' AND EndsUtc IS NULL;
CREATE UNIQUE INDEX TD_ManagedControl ON CharacterRoleControls(CharacterID,RoleAssignmentID,TeamManagementID) WHERE OriginKind='TEAM_MANAGEMENT' AND EndsUtc IS NULL;
CREATE UNIQUE INDEX TD_SeasonCompetition ON CompetitionSeasons(WorldCompetitionID,CompetitionSeasonID);
CREATE TABLE LeagueGameCalendars (
    CalendarID INTEGER PRIMARY KEY,
)TD_SQL"
R"TD_SQL(
    WorldID INTEGER NOT NULL,
    WorldCompetitionID INTEGER NOT NULL,
    CalendarKey TEXT NOT NULL,
    PolicyVersion TEXT NOT NULL,
    UNIQUE(WorldID,CalendarKey,PolicyVersion),
    UNIQUE(WorldID,CalendarID), UNIQUE(WorldID,CalendarID,WorldCompetitionID),
    FOREIGN KEY(WorldID,WorldCompetitionID) REFERENCES WorldCompetitions(WorldID,WorldCompetitionID)
);
CREATE TABLE GameWeekPeriods (
    HistoryWeekPeriodID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    CalendarID INTEGER NOT NULL,
    WorldCompetitionID INTEGER NOT NULL,
    CompetitionSeasonID INTEGER,
    PeriodKey TEXT NOT NULL,
    PhaseCode TEXT NOT NULL CHECK(PhaseCode IN ('PRESEASON','REGULAR','POSTSEASON','OFFSEASON')),
    DisplayWeekNumber INTEGER,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT NOT NULL,
    WorldCoreSnapshotID INTEGER,
    CHECK(StartsUtc<EndsUtc),
    CHECK(PhaseCode<>'REGULAR' OR (CompetitionSeasonID IS NOT NULL AND DisplayWeekNumber IS NOT NULL AND DisplayWeekNumber>0)),
    UNIQUE(WorldID,CalendarID,PeriodKey), UNIQUE(WorldID,CalendarID,StartsUtc),
    UNIQUE(WorldID,CalendarID,HistoryWeekPeriodID),
    FOREIGN KEY(WorldID,CalendarID,WorldCompetitionID) REFERENCES LeagueGameCalendars(WorldID,CalendarID,WorldCompetitionID),
    FOREIGN KEY(WorldCompetitionID,CompetitionSeasonID) REFERENCES CompetitionSeasons(WorldCompetitionID,CompetitionSeasonID),
    FOREIGN KEY(WorldID,WorldCoreSnapshotID) REFERENCES WorldCoreSnapshots(WorldID,WorldCoreSnapshotID)
);
CREATE TABLE VenueCalendarBindings (
    WorldID INTEGER NOT NULL,
    WorldVenueID INTEGER NOT NULL,
    CalendarID INTEGER NOT NULL,
    PRIMARY KEY(WorldID,WorldVenueID), UNIQUE(WorldID,WorldVenueID,CalendarID),
    FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID),
    FOREIGN KEY(WorldID,CalendarID) REFERENCES LeagueGameCalendars(WorldID,CalendarID)
);
CREATE TABLE VenueHistoryBaselines (
    WorldID INTEGER NOT NULL,
    WorldVenueID INTEGER NOT NULL,
    VenueName TEXT NOT NULL CHECK(length(trim(VenueName))>0),
    TurfType TEXT,
    RecordedUtc TEXT NOT NULL,
    RecordedRevision INTEGER NOT NULL CHECK(RecordedRevision>=0),
    PRIMARY KEY(WorldID,WorldVenueID),
    FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID)
);
CREATE TABLE VenueWeekHistory (
    WorldID INTEGER NOT NULL,
    WorldVenueID INTEGER NOT NULL,
    CalendarID INTEGER NOT NULL,
    HistoryWeekPeriodID INTEGER NOT NULL,
    OpeningName TEXT NOT NULL CHECK(length(trim(OpeningName))>0),
    OpeningTurfType TEXT,
    FinalName TEXT NOT NULL CHECK(length(trim(FinalName))>0),
    FinalTurfType TEXT,
    FirstChangedUtc TEXT NOT NULL,
    LastEditedUtc TEXT NOT NULL,
    FirstRevision INTEGER NOT NULL CHECK(FirstRevision>=0),
    LastRevision INTEGER NOT NULL CHECK(LastRevision>=FirstRevision),
    ClosedAtUtc TEXT,
    ClosedRevision INTEGER,
    HasNetChange INTEGER GENERATED ALWAYS AS
        (OpeningName IS NOT FinalName OR OpeningTurfType IS NOT FinalTurfType) VIRTUAL,
    CHECK((ClosedAtUtc IS NULL AND ClosedRevision IS NULL) OR (ClosedAtUtc IS NOT NULL AND ClosedRevision IS NOT NULL AND ClosedRevision>=LastRevision)),
    CHECK(LastEditedUtc>=FirstChangedUtc),
)TD_SQL"
R"TD_SQL(
    PRIMARY KEY(WorldID,WorldVenueID,HistoryWeekPeriodID),
    FOREIGN KEY(WorldID,WorldVenueID) REFERENCES VenueHistoryBaselines(WorldID,WorldVenueID),
    FOREIGN KEY(WorldID,WorldVenueID,CalendarID) REFERENCES VenueCalendarBindings(WorldID,WorldVenueID,CalendarID),
    FOREIGN KEY(WorldID,CalendarID,HistoryWeekPeriodID) REFERENCES GameWeekPeriods(WorldID,CalendarID,HistoryWeekPeriodID)
);
CREATE TABLE GameVenueEvidence (
    GameID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    WorldVenueID INTEGER,
    EvidenceKind TEXT NOT NULL CHECK(EvidenceKind IN ('CAPTURED','UNKNOWN_VENUE')),
    VenueName TEXT,
    TurfType TEXT,
    CapturedUtc TEXT NOT NULL,
    DataRevision INTEGER NOT NULL CHECK(DataRevision>=0),
    CHECK((EvidenceKind='CAPTURED' AND WorldVenueID IS NOT NULL AND VenueName IS NOT NULL AND length(trim(VenueName))>0)
       OR (EvidenceKind='UNKNOWN_VENUE' AND WorldVenueID IS NULL AND VenueName IS NULL AND TurfType IS NULL)),
    FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID),
    FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID)
);
CREATE TRIGGER TD_PeriodOverlapInsert BEFORE INSERT ON GameWeekPeriods
WHEN EXISTS(SELECT 1 FROM GameWeekPeriods p WHERE p.WorldID=NEW.WorldID AND p.CalendarID=NEW.CalendarID
    AND p.StartsUtc<NEW.EndsUtc AND NEW.StartsUtc<p.EndsUtc)
BEGIN SELECT RAISE(ABORT,'game-week period overlap'); END;
CREATE TRIGGER TD_PeriodUpdate BEFORE UPDATE ON GameWeekPeriods
BEGIN SELECT RAISE(ABORT,'calendar periods are immutable; add future periods'); END;
CREATE TRIGGER TD_PeriodDelete BEFORE DELETE ON GameWeekPeriods
BEGIN SELECT RAISE(ABORT,'calendar periods are immutable'); END;
CREATE TRIGGER TD_BaselineUpdate BEFORE UPDATE ON VenueHistoryBaselines
BEGIN SELECT RAISE(ABORT,'venue baseline is immutable'); END;
CREATE TRIGGER TD_BaselineDelete BEFORE DELETE ON VenueHistoryBaselines
BEGIN SELECT RAISE(ABORT,'venue baseline is immutable'); END;
CREATE TRIGGER TD_VenueWeekInsert BEFORE INSERT ON VenueWeekHistory
WHEN NEW.ClosedAtUtc IS NOT NULL OR NEW.ClosedRevision IS NOT NULL OR NOT EXISTS(SELECT 1 FROM GameWeekPeriods p JOIN Worlds w ON w.WorldID=p.WorldID
 WHERE p.WorldID=NEW.WorldID AND p.CalendarID=NEW.CalendarID AND p.HistoryWeekPeriodID=NEW.HistoryWeekPeriodID
 AND p.StartsUtc<=w.CurrentTimeUtc AND w.CurrentTimeUtc<p.EndsUtc
 AND p.StartsUtc<=NEW.FirstChangedUtc AND NEW.LastEditedUtc<p.EndsUtc
 AND NEW.LastEditedUtc=w.CurrentTimeUtc)
BEGIN SELECT RAISE(ABORT,'venue history must use current simulation game week'); END;
CREATE TRIGGER TD_VenueWeekImmutable BEFORE UPDATE ON VenueWeekHistory
WHEN OLD.ClosedAtUtc IS NOT NULL OR NEW.WorldID<>OLD.WorldID OR NEW.WorldVenueID<>OLD.WorldVenueID
 OR NEW.CalendarID<>OLD.CalendarID OR NEW.HistoryWeekPeriodID<>OLD.HistoryWeekPeriodID
 OR NEW.OpeningName IS NOT OLD.OpeningName OR NEW.OpeningTurfType IS NOT OLD.OpeningTurfType
 OR NEW.FirstChangedUtc<>OLD.FirstChangedUtc OR NEW.FirstRevision<>OLD.FirstRevision
BEGIN SELECT RAISE(ABORT,'closed history or immutable opening values cannot change'); END;
CREATE TRIGGER TD_VenueWeekEdit BEFORE UPDATE ON VenueWeekHistory
WHEN NEW.ClosedAtUtc IS NULL AND (NEW.LastRevision<=OLD.LastRevision OR NOT EXISTS
 (SELECT 1 FROM GameWeekPeriods p JOIN Worlds w ON w.WorldID=p.WorldID
 WHERE p.WorldID=NEW.WorldID AND p.CalendarID=NEW.CalendarID AND p.HistoryWeekPeriodID=NEW.HistoryWeekPeriodID
 AND p.StartsUtc<=w.CurrentTimeUtc AND w.CurrentTimeUtc<p.EndsUtc AND NEW.LastEditedUtc=w.CurrentTimeUtc))
BEGIN SELECT RAISE(ABORT,'venue edit must use the current simulation period and a new revision'); END;
CREATE TRIGGER TD_VenueWeekClose BEFORE UPDATE ON VenueWeekHistory
WHEN NEW.ClosedAtUtc IS NOT NULL AND (NEW.FinalName IS NOT OLD.FinalName
 OR NEW.FinalTurfType IS NOT OLD.FinalTurfType OR NEW.LastEditedUtc<>OLD.LastEditedUtc
 OR NEW.LastRevision<>OLD.LastRevision OR NOT EXISTS
 (SELECT 1 FROM GameWeekPeriods p JOIN Worlds w ON w.WorldID=p.WorldID
 WHERE p.WorldID=NEW.WorldID AND p.CalendarID=NEW.CalendarID AND p.HistoryWeekPeriodID=NEW.HistoryWeekPeriodID
 AND NEW.ClosedAtUtc=p.EndsUtc AND w.CurrentTimeUtc>=p.EndsUtc))
BEGIN SELECT RAISE(ABORT,'period closure must preserve the final saved venue values'); END;
CREATE TRIGGER TD_VenueWeekDelete BEFORE DELETE ON VenueWeekHistory
BEGIN SELECT RAISE(ABORT,'venue history is retained, including net-zero summaries'); END;
CREATE TRIGGER TD_GameVenueUpdate BEFORE UPDATE ON GameVenueEvidence
BEGIN SELECT RAISE(ABORT,'captured game venue evidence is immutable'); END;
CREATE TRIGGER TD_GameVenueDelete BEFORE DELETE ON GameVenueEvidence
BEGIN SELECT RAISE(ABORT,'captured game venue evidence is immutable'); END;
CREATE TRIGGER TD_ControlInsert BEFORE INSERT ON CharacterRoleControls
)TD_SQL"
R"TD_SQL(
WHEN NEW.EndsUtc IS NULL AND (
 NOT EXISTS(SELECT 1 FROM Characters c WHERE c.CharacterID=NEW.CharacterID AND c.EndsUtc IS NULL)
 OR NOT EXISTS(SELECT 1 FROM RoleAssignments a WHERE a.RoleAssignmentID=NEW.RoleAssignmentID AND a.EndsUtc IS NULL)
 OR (NEW.OriginKind='TEAM_MANAGEMENT' AND NOT EXISTS
     (SELECT 1 FROM TeamManagementIntents i WHERE i.TeamManagementID=NEW.TeamManagementID AND i.EndsUtc IS NULL)))
BEGIN SELECT RAISE(ABORT,'control requires active character, assignment and management intent'); END;
CREATE TRIGGER TD_ControlReopen BEFORE UPDATE ON CharacterRoleControls
WHEN NEW.EndsUtc IS NULL AND (
 NOT EXISTS(SELECT 1 FROM Characters c WHERE c.CharacterID=NEW.CharacterID AND c.EndsUtc IS NULL)
 OR NOT EXISTS(SELECT 1 FROM RoleAssignments a WHERE a.RoleAssignmentID=NEW.RoleAssignmentID AND a.EndsUtc IS NULL)
 OR (NEW.OriginKind='TEAM_MANAGEMENT' AND NOT EXISTS
     (SELECT 1 FROM TeamManagementIntents i WHERE i.TeamManagementID=NEW.TeamManagementID AND i.EndsUtc IS NULL)))
BEGIN SELECT RAISE(ABORT,'control requires active references'); END;
CREATE TRIGGER TD_AssignmentClose BEFORE UPDATE OF EndsUtc ON RoleAssignments
WHEN NEW.EndsUtc IS NOT NULL AND EXISTS
 (SELECT 1 FROM CharacterRoleControls g WHERE g.RoleAssignmentID=OLD.RoleAssignmentID AND g.EndsUtc IS NULL)
BEGIN SELECT RAISE(ABORT,'close role control grants before assignment'); END;
CREATE TRIGGER TD_CharacterClose BEFORE UPDATE OF EndsUtc ON Characters
WHEN NEW.EndsUtc IS NOT NULL AND EXISTS
 (SELECT 1 FROM CharacterRoleControls g WHERE g.CharacterID=OLD.CharacterID AND g.EndsUtc IS NULL)
BEGIN SELECT RAISE(ABORT,'close role controls before character'); END;
CREATE TRIGGER TD_ManagementClose BEFORE UPDATE OF EndsUtc ON TeamManagementIntents
WHEN NEW.EndsUtc IS NOT NULL AND EXISTS
 (SELECT 1 FROM CharacterRoleControls g WHERE g.TeamManagementID=OLD.TeamManagementID AND g.EndsUtc IS NULL)
BEGIN SELECT RAISE(ABORT,'close managed grants before management intent'); END;

CREATE TABLE OrganizationIdentityOverrides (
    WorldID INTEGER NOT NULL,
    WorldOrganizationID INTEGER NOT NULL,
    MarketName TEXT NOT NULL CHECK(length(trim(MarketName))>0),
    Nickname TEXT NOT NULL CHECK(length(trim(Nickname))>0),
    DisplayName TEXT NOT NULL CHECK(length(trim(DisplayName))>0),
    ChangedUtc TEXT NOT NULL,
    ChangedRevision INTEGER NOT NULL CHECK(ChangedRevision>=0),
    PRIMARY KEY(WorldID,WorldOrganizationID),
    FOREIGN KEY(WorldID,WorldOrganizationID) REFERENCES WorldOrganizations(WorldID,WorldOrganizationID)
);



)TD_SQL";
