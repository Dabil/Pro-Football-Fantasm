#pragma once
#include <string>
// CLOCK-SCHEDULER-STAGE1-2026-11-04. Fresh construction only; CSD-001..012.
inline std::string clockSchedulerWorldSchema()
{
    std::string sql;
    sql += R"CLOCK_SQL(-- CLOCK-SCHEDULER-DESIGN-2026-11-04. Adopted fresh World5 construction only.
-- Execute after baseline World DDL in a NEW EMPTY DB; this is NOT a World4 migration.
-- Retained World4 definitions below are composition inputs, followed by reviewed replacements.
-- Includes the retained identity/control/week baseline, followed by clock/event
-- replacements below. See Clock_and_Scheduler_Database_and_Service_Design_v0.1.md.
-- Runtime adoption: CLOCK-SCHEDULER-STAGE1-2026-11-04; game/window driver remains gated.
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
    Revision INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((Revision>=0),0))
);
CREATE TABLE WorldUsers (
    WorldUserID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
    UserKey TEXT NOT NULL,
    DisplayName TEXT NOT NULL CHECK(COALESCE((length(trim(DisplayName))>0),0)),
    CanManageTeams INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((CanManageTeams IN (0,1)),0)),
    CanAdvanceWorld INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((CanAdvanceWorld IN (0,1)),0)),
    CanEnableGodMode INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((CanEnableGodMode IN (0,1)),0)),
    Active INTEGER NOT NULL DEFAULT 1 CHECK(COALESCE((Active IN (0,1)),0)),
    UNIQUE(WorldID,UserKey), UNIQUE(WorldID,WorldUserID)
);
CREATE TABLE WorldUserEditModes (
    WorldID INTEGER NOT NULL,
    WorldUserID INTEGER NOT NULL,
    GodModeEnabled INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((GodModeEnabled IN (0,1)),0)),
    ChangedUtc TEXT NOT NULL,
    ChangedRevision INTEGER NOT NULL CHECK(COALESCE((ChangedRevision>=0),0)),
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
    ProfileVersion INTEGER NOT NULL CHECK(COALESCE((ProfileVersion>0),0)),
    UNIQUE(WorldID,ProfileKey,ProfileVersion), UNIQUE(WorldID,AuthorityProfileID)
);
CREATE TABLE AuthorityProfileCapabilities (
    WorldID INTEGER NOT NULL,
    AuthorityProfileID INTEGER NOT NULL,
    CapabilityKey TEXT NOT NULL,
    PRIMARY KEY(WorldID,AuthorityProfileID,CapabilityKey),

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
    ScopeKind TEXT NOT NULL CHECK(COALESCE((ScopeKind IN ('ORGANIZATION','COMPETITION','WORLD')),0)),
    WorldOrganizationID INTEGER,
    WorldCompetitionID INTEGER,
    TeamSeasonID INTEGER,
    PositionKey TEXT NOT NULL,
    RoleID INTEGER NOT NULL,
    AuthorityProfileID INTEGER,
    Active INTEGER NOT NULL DEFAULT 1 CHECK(COALESCE((Active IN (0,1)),0)),
    CHECK(COALESCE(((ScopeKind='ORGANIZATION' AND WorldOrganizationID IS NOT NULL AND WorldCompetitionID IS NULL)
       OR (ScopeKind='COMPETITION' AND WorldOrganizationID IS NULL AND WorldCompetitionID IS NOT NULL AND TeamSeasonID IS NULL)
       OR (ScopeKind='WORLD' AND WorldOrganizationID IS NULL AND WorldCompetitionID IS NULL AND TeamSeasonID IS NULL)),0)),
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
    StartsRevision INTEGER NOT NULL CHECK(COALESCE((StartsRevision>=0),0)),
    EndsUtc TEXT,
    EndsRevision INTEGER,
    EndReasonCode TEXT,
    CHECK(COALESCE(((EndsUtc IS NULL AND EndsRevision IS NULL AND EndReasonCode IS NULL)
       OR (EndsUtc IS NOT NULL AND EndsRevision IS NOT NULL AND EndReasonCode IS NOT NULL
           AND EndsUtc>=StartsUtc AND EndsRevision>=StartsRevision)),0)),
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
    CHECK(COALESCE((FirstRoleID<=SecondRoleID),0)),
    PRIMARY KEY(WorldID,FirstRoleID,SecondRoleID),
    FOREIGN KEY(WorldID,FirstRoleID) REFERENCES RoleDefinitions(WorldID,RoleID),
    FOREIGN KEY(WorldID,SecondRoleID) REFERENCES RoleDefinitions(WorldID,RoleID)
);
CREATE TABLE Characters (
    CharacterID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,

    WorldUserID INTEGER NOT NULL,
    PersonID INTEGER NOT NULL,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT,
    CHECK(COALESCE((EndsUtc IS NULL OR EndsUtc>=StartsUtc),0)),
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
    CHECK(COALESCE((EndsUtc IS NULL OR EndsUtc>=StartsUtc),0)),
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
    OriginKind TEXT NOT NULL CHECK(COALESCE((OriginKind IN ('MANUAL','TEAM_MANAGEMENT')),0)),
    TeamManagementID INTEGER,
    StartsUtc TEXT NOT NULL,
    EndsUtc TEXT,
    CHECK(COALESCE((EndsUtc IS NULL OR EndsUtc>=StartsUtc),0)),
    CHECK(COALESCE(((OriginKind='MANUAL' AND TeamManagementID IS NULL)
       OR (OriginKind='TEAM_MANAGEMENT' AND TeamManagementID IS NOT NULL)),0)),
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
CREATE TABLE GameVenueEvidence (
    GameID INTEGER PRIMARY KEY,
    WorldID INTEGER NOT NULL,
    WorldVenueID INTEGER,
    EvidenceKind TEXT NOT NULL CHECK(COALESCE((EvidenceKind IN ('CAPTURED','UNKNOWN_VENUE')),0)),
    VenueName TEXT,
    TurfType TEXT,
    CapturedUtc TEXT NOT NULL,
    DataRevision INTEGER NOT NULL CHECK(COALESCE((DataRevision>=0),0)),
    CHECK(COALESCE(((EvidenceKind='CAPTURED' AND WorldVenueID IS NOT NULL AND VenueName IS NOT NULL AND length(trim(VenueName))>0)
       OR (EvidenceKind='UNKNOWN_VENUE' AND WorldVenueID IS NULL AND VenueName IS NULL AND TurfType IS NULL)),0)),
    FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID),
    FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID)
);
CREATE TRIGGER TD_GameVenueUpdate BEFORE UPDATE ON GameVenueEvidence
BEGIN SELECT RAISE(ABORT,'captured game venue evidence is immutable'); END;
CREATE TRIGGER TD_GameVenueDelete BEFORE DELETE ON GameVenueEvidence
BEGIN SELECT RAISE(ABORT,'captured game venue evidence is immutable'); END;
CREATE TRIGGER TD_ControlInsert BEFORE INSERT ON CharacterRoleControls

WHEN NEW.EndsUtc IS NULL AND (
 NOT EXISTS(SELECT 1 FROM Characters c WHERE c.CharacterID=NEW.CharacterID AND c.EndsUtc IS NULL)
 OR NOT EXISTS(SELECT 1 FROM RoleAssignments a WHERE a.RoleAssignmentID=NEW.RoleAssignmentID AND a.EndsUtc IS NULL)
 OR (NEW.OriginKind=')CLOCK_SQL";
    sql += R"CLOCK_SQL(TEAM_MANAGEMENT' AND NOT EXISTS
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
    MarketName TEXT NOT NULL CHECK(COALESCE((length(trim(MarketName))>0),0)),
    Nickname TEXT NOT NULL CHECK(COALESCE((length(trim(Nickname))>0),0)),
    DisplayName TEXT NOT NULL CHECK(COALESCE((length(trim(DisplayName))>0),0)),
    ChangedUtc TEXT NOT NULL,
    ChangedRevision INTEGER NOT NULL CHECK(COALESCE((ChangedRevision>=0),0)),
    PRIMARY KEY(WorldID,WorldOrganizationID),
    FOREIGN KEY(WorldID,WorldOrganizationID) REFERENCES WorldOrganizations(WorldID,WorldOrganizationID)
);




-- Independent league-week/result definitions retained from schema 3.
CREATE UNIQUE INDEX VE_SeasonScope ON CompetitionSeasons(WorldCompetitionID,CompetitionSeasonID);
CREATE UNIQUE INDEX VE_ScheduledSeason ON WorldScheduledGames(CompetitionSeasonID,WorldScheduledGameID);
CREATE TABLE LeagueWeeks (
 LeagueWeekID INTEGER PRIMARY KEY,
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
 WorldCompetitionID INTEGER NOT NULL,
 CompetitionSeasonID INTEGER NOT NULL,
 WeekKey TEXT NOT NULL CHECK(COALESCE((COALESCE(length(trim(WeekKey))>0,0)),0)),
 PhaseCode TEXT NOT NULL,
 SequenceNo INTEGER NOT NULL CHECK(COALESCE((COALESCE(SequenceNo>0,0)),0)),
 DisplayWeekNumber INTEGER,
 NextLeagueWeekID INTEGER,
 State TEXT NOT NULL CHECK(COALESCE((COALESCE(State IN ('FUTURE','OPEN','CLOSED'),0)),0)),
 OpenedUtc TEXT, ClosedUtc TEXT,
 OpenedRevision INTEGER, ClosedRevision INTEGER,
 UNIQUE(WorldID,CompetitionSeasonID,LeagueWeekID),
 UNIQUE(WorldID,CompetitionSeasonID,WeekKey),
 UNIQUE(WorldID,CompetitionSeasonID,SequenceNo),
 FOREIGN KEY(WorldID,WorldCompetitionID) REFERENCES WorldCompetitions(WorldID,WorldCompetitionID),
 FOREIGN KEY(WorldCompetitionID,CompetitionSeasonID) REFERENCES CompetitionSeasons(WorldCompetitionID,CompetitionSeasonID),
 FOREIGN KEY(WorldID,CompetitionSeasonID,NextLeagueWeekID) REFERENCES LeagueWeeks(WorldID,CompetitionSeasonID,LeagueWeekID),
 CHECK(COALESCE((COALESCE(NextLeagueWeekID IS NULL OR NextLeagueWeekID<>LeagueWeekID,0)),0)),
 CHECK(COALESCE((COALESCE((State='FUTURE' AND OpenedUtc IS NULL AND ClosedUtc IS NULL AND OpenedRevision IS NULL AND ClosedRevision IS NULL)
 OR (State='OPEN' AND OpenedUtc IS NOT NULL AND OpenedRevision>=0 AND ClosedUtc IS NULL AND ClosedRevision IS NULL)
 OR (State='CLOSED' AND OpenedUtc IS NOT NULL AND ClosedUtc>=OpenedUtc AND OpenedRevision>=0 AND ClosedRevision>=OpenedRevision),0)),0))
);
CREATE UNIQUE INDEX VE_OneOpenWeek ON LeagueWeeks(WorldID,CompetitionSeasonID) WHERE State='OPEN';
CREATE TABLE LeagueWeekGames (
 WorldID INTEGER NOT NULL,
 CompetitionSeasonID INTEGER NOT NULL,
 LeagueWeekID INTEGER NOT NULL,
 WorldScheduledGameID INTEGER NOT NULL,
 PRIMARY KEY(WorldID,WorldScheduledGameID),
 FOREIGN KEY(WorldID,CompetitionSeasonID,LeagueWeekID) REFERENCES LeagueWeeks(WorldID,CompetitionSeasonID,LeagueWeekID),
 FOREIGN KEY(CompetitionSeasonID,WorldScheduledGameID) REFERENCES WorldScheduledGames(CompetitionSeasonID,WorldScheduledGameID)
);
CREATE TABLE FinalizedLeagueGameResults (
 WorldID INTEGER NOT NULL,
 WorldScheduledGameID INTEGER NOT NULL,
 GameID INTEGER NOT NULL,
 FinalizationKey TEXT NOT NULL CHECK(COALESCE((COALESCE(length(trim(FinalizationKey))>0,0)),0)),
 ResultFingerprint TEXT NOT NULL CHECK(COALESCE((COALESCE(length(trim(ResultFingerprint))>0,0)),0)),
 CompletedUtc TEXT NOT NULL,
 FinalizedRevision INTEGER NOT NULL CHECK(COALESCE((COALESCE(FinalizedRevision>0,0)),0)),
 PRIMARY KEY(WorldID,WorldScheduledGameID),
 UNIQUE(WorldID,GameID), UNIQUE(WorldID,FinalizationKey),
 FOREIGN KEY(WorldID,WorldScheduledGameID) REFERENCES LeagueWeekGames(WorldID,WorldScheduledGameID),
 FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID)
);
CREATE TABLE LeagueWeekTransitions (
 TransitionID INTEGER PRIMARY KEY,
 WorldID INTEGER NOT NULL,
 CompetitionSeasonID INTEGER NOT NULL,
 FromLeagueWeekID INTEGER NOT NULL,
 ToLeagueWeekID INTEGER,
 TriggerGameID INTEGER NOT NULL,
 OccurredUtc TEXT NOT NULL,
 DataRevision INTEGER NOT NULL CHECK(COALESCE((COALESCE(DataRevision>0,0)),0)),
 UNIQUE(WorldID,TransitionID), UNIQUE(WorldID,FromLeagueWeekID),
 FOREIGN KEY(WorldID,CompetitionSeasonID,FromLeagueWeekID) REFERENCES LeagueWeeks(WorldID,CompetitionSeasonID,LeagueWeekID),
 FOREIGN KEY(WorldID,CompetitionSeasonID,ToLeagueWeekID) REFERENCES LeagueWeeks(WorldID,CompetitionSeasonID,LeagueWeekID),
 FOREIGN KEY(WorldID,TriggerGameID) REFERENCES Games(WorldID,GameID),
 CHECK(COALESCE((COALESCE(ToLeagueWeekID IS NULL OR ToLeagueWeekID<>FromLeagueWeekID,0)),0))
);







CREATE TRIGGER VE_ResultImmutable BEFORE UPDATE ON FinalizedLeagueGameResults BEGIN SELECT RAISE(ABORT,'finalized result receipt is immutable'); END;
CREATE TRIGGER VE_ResultNoDelete BEFORE DELETE ON FinalizedLeagueGameResults BEGIN SELECT RAISE(ABORT,'retain finalized result receipt'); END;
CREATE TRIGGER VE_ResultMatchesGame BEFORE INSERT ON FinalizedLeagueGameResults
WHEN NOT EXISTS(SELECT 1 FROM Games g JOIN GameVenueEvidence e ON e.WorldID=g.WorldID AND e.GameID=g.GameID
 WHERE g.WorldID=NEW.WorldID AND g.GameID=NEW.GameID AND g.WorldScheduledGameID=NEW.WorldScheduledGameID
 AND g.GameStatus='FINAL' AND g.CompletedUtc=NEW.CompletedUtc)
BEGIN SELECT RAISE(ABORT,'result receipt requires matching final game and venue evidence'); END;
CREATE TRIGGER VE_CloseWeekReady BEFORE UPDATE OF State ON LeagueWeeks
WHEN NEW.State='CLOSED' AND (OLD.State<>'OPEN' OR NOT EXISTS(SELECT 1 FROM LeagueWeekGames x WHERE x.WorldID=OLD.WorldID AND x.LeagueWeekID=OLD.LeagueWeekID)
 OR EXISTS(SELECT 1 FROM LeagueWeekGames x WHERE x.WorldID=OLD.WorldID AND x.LeagueWeekID=OLD.LeagueWeekID
 AND NOT EXISTS(SELECT 1 FROM FinalizedLeagueGameResults r WHERE r.WorldID=x.WorldID AND r.WorldScheduledGameID=x.WorldScheduledGameID)))
BEGIN SELECT RAISE(ABORT,'week requires all nonempty required games finalized'); END;
CREATE TRIGGER VE_ClosedWeekImmutable BEFORE UPDATE ON LeagueWeeks WHEN OLD.State='CLOSED'
BEGIN SELECT RAISE(ABORT,'closed league week is immutable'); END;
CREATE TRIGGER VE_WeekNoDelete BEFORE DELETE ON LeagueWeeks BEGIN SELECT RAISE(ABORT,'retain league week identity'); END;
CREATE TRIGGER VE_TransitionImmutable BEFORE UPDATE ON LeagueWeekTransitions BEGIN SELECT RAISE(ABORT,'week transition is immutable'); END;
CREATE TRIGGER VE_TransitionNoDelete BEFORE DELETE ON LeagueWeekTransitions BEGIN SELECT RAISE(ABORT,'retain week transition'); END;



CREATE TRIGGER VE_WeekMemberInsert BEFORE INSERT ON LeagueWeekGames
WHEN NOT EXISTS(SELECT 1 FROM LeagueWeeks w WHERE w.WorldID=NEW.WorldID AND w.LeagueWeekID=NEW.LeagueWeekID AND w.State='FUTURE')
BEGIN SELECT RAISE(ABORT,'populate required games before week opens'); END;
CREATE TRIGGER VE_WeekMemberUpdate BEFORE UPDATE ON LeagueWeekGames BEGIN SELECT RAISE(ABORT,'required game membership is immutable'); END;
CREATE TRIGGER VE_WeekMemberDelete BEFORE DELETE ON LeagueWeekGames BEGIN SELECT RAISE(ABORT,'required games cannot silently disappear'); END;
CREATE TRIGGER VE_TransitionScope BEFORE INSERT ON LeagueWeekTransitions
WHEN NOT EXISTS(SELECT 1 FROM LeagueWeeks w JOIN LeagueWeekGames m ON m.WorldID=w.WorldID AND m.LeagueWeekID=w.LeagueWeekID
 JOIN FinalizedLeagueGameResults r ON r.WorldID=m.WorldID AND r.WorldScheduledGameID=m.WorldScheduledGameID
 WHERE w.WorldID=NEW.WorldID AND w.CompetitionSeasonID=NEW.CompetitionSeasonID AND w.LeagueWeekID=NEW.FromLeagueWeekID
 AND w.State='CLOSED' AND w.NextLeagueWeekID IS NEW.ToLeagueWeekID AND w.ClosedUtc=NEW.OccurredUtc
 AND w.ClosedRevision=NEW.DataRevision AND r.GameID=NEW.TriggerGameID AND r.FinalizedRevision=NEW.DataRevision)
BEGIN SELECT RAISE(ABORT,'transition requires matching completed week and last result'); END;



CREATE TABLE WorldFeatureVersions (
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
 FeatureKey TEXT NOT NULL CHECK(COALESCE((length(trim(FeatureKey))>0),0)),
 ContractVersion INTEGER NOT NULL CHECK(COALESCE((ContractVersion>0),0)),
 PRIMARY KEY(WorldID,FeatureKey)
);



CREATE TABLE WorldEventKinds (
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
 KindKey TEXT NOT NULL,
 HandlerVersion INTEGER NOT NULL CHECK(COALESCE((COALESCE((HandlerVersion>0),0)),0)),
 PhaseCode INTEGER NOT NULL CHECK(COALESCE((COALESCE((PhaseCode IN (1,3)),0)),0)),
 PRIMARY KEY(WorldID,KindKey),
 CHECK(COALESCE((COALESCE(((KindKey IN ('VENUE_APPLY','SEASON_EVENT','TRADE_EVENT','VENUE_CEREMONY') AND PhaseCode=1) OR (KindKey='GAME_KICKOFF' AND PhaseCode=3)),0)),0))
);
CREATE TABLE ScheduledWorldEvents (
 ScheduledWorldEventID INTEGER PRIMARY KEY,
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID),
 EventKey TEXT NOT NULL CHECK(COALESCE((COALESCE((length(trim(EventKey))>0),0)),0)),
 KindKey TEXT NOT NULL,
 StartsUtc TEXT NOT NULL,
 EndMode TEXT NOT NULL CHECK(COALESCE((COALESCE((EndMode IN ('INSTANT','FIXED','DOMAIN')),0)),0)),
 EndsUtc TEXT,
 CreatedUtc TEXT NOT NULL,
 CreatedRevision INTEGER NOT NULL CHECK(COALESCE((COALESCE((CreatedRevision>=0),0)),0)),
 Visibility TEXT NOT NULL CHECK(COALESCE((COALESCE((Visibility IN ('PUBLIC','RESTRICTED','INTERNAL')),0)),0)),
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('DRAFT','PENDING','RUNNING','COMPLETED','CANCELED','SUPERSEDED')),0)),0)),
 SupersedesEventID INTEGER,
 StartedUtc TEXT,
 ResolvedUtc TEXT, ResolvedRevision INTEGER,
 ResolutionReason TEXT,
 UNIQUE(WorldID,ScheduledWorldEventID), UNIQUE(WorldID,EventKey),
 FOREIGN KEY(WorldID,KindKey) REFERENCES WorldEventKinds(WorldID,KindKey),
 FOREIGN KEY(WorldID,SupersedesEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 CHECK(COALESCE((COALESCE((COALESCE(strftime('%Y-%m-%dT%H:%M:%SZ',StartsUtc)=StartsUtc AND length(StartsUtc)=20 AND CreatedUtc<=StartsUtc,0)),0)),0)),
 CHECK(COALESCE((COALESCE(((EndMode='INSTANT' AND EndsUtc IS NULL) OR (EndMode='FIXED' AND EndsUtc>StartsUtc) OR (EndMode='DOMAIN' AND EndsUtc IS NULL)),0)),0)),
 CHECK(COALESCE((COALESCE((COALESCE((State IN ('DRAFT','PENDING') AND StartedUtc IS NULL AND ResolvedUtc IS NULL AND ResolvedRevision IS NULL AND ResolutionReason IS NULL)
 OR (State='RUNNING' AND StartedUtc=StartsUtc AND ResolvedUtc IS NULL AND ResolvedRevision IS NULL AND ResolutionReason IS NULL)
 OR (State='COMPLETED' AND StartedUtc>=StartsUtc AND ResolvedUtc>=StartedUtc AND )CLOCK_SQL";
    sql += R"CLOCK_SQL(ResolvedRevision>=CreatedRevision AND ResolutionReason IS NOT NULL)
 OR (State IN ('CANCELED','SUPERSEDED') AND StartedUtc IS NULL AND ResolvedUtc>=CreatedUtc AND ResolvedRevision>=CreatedRevision AND ResolutionReason IS NOT NULL),0)),0)),0)),
 CHECK(COALESCE((COALESCE((StartsUtc IS NULL OR (length(StartsUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',StartsUtc)=StartsUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((EndsUtc IS NULL OR (length(EndsUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',EndsUtc)=EndsUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((CreatedUtc IS NULL OR (length(CreatedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',CreatedUtc)=CreatedUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((StartedUtc IS NULL OR (length(StartedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',StartedUtc)=StartedUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((ResolvedUtc IS NULL OR (length(ResolvedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',ResolvedUtc)=ResolvedUtc)),0)),0))
);
CREATE INDEX WS_Due ON ScheduledWorldEvents(WorldID,StartsUtc,KindKey,ScheduledWorldEventID) WHERE State='PENDING';
CREATE TABLE WorldEventReaders (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, WorldUserID INTEGER NOT NULL,
 KnownFromUtc TEXT NOT NULL,
 PRIMARY KEY(WorldID,ScheduledWorldEventID,WorldUserID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 CHECK(COALESCE((COALESCE((KnownFromUtc IS NULL OR (length(KnownFromUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',KnownFromUtc)=KnownFromUtc)),0)),0))
);
CREATE TABLE GameKickoffOccurrences (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 CompetitionSeasonID INTEGER NOT NULL, WorldScheduledGameID INTEGER NOT NULL,
 IsCurrent INTEGER NOT NULL DEFAULT 1 CHECK(COALESCE((COALESCE((IsCurrent IN (0,1)),0)),0)),
 PRIMARY KEY(WorldID,ScheduledWorldEventID),
 UNIQUE(WorldID,WorldScheduledGameID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(CompetitionSeasonID,WorldScheduledGameID) REFERENCES WorldScheduledGames(CompetitionSeasonID,WorldScheduledGameID)
);
CREATE UNIQUE INDEX WS_CurrentKickoff ON GameKickoffOccurrences(WorldID,WorldScheduledGameID) WHERE IsCurrent=1;
CREATE TABLE RunningGameEvents (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, GameID INTEGER NOT NULL,
 PRIMARY KEY(WorldID,ScheduledWorldEventID), UNIQUE(WorldID,GameID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES GameKickoffOccurrences(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID)
);
CREATE TABLE WorldGameCheckpoints (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 Generation INTEGER NOT NULL CHECK(COALESCE((COALESCE((Generation>=0),0)),0)),
 NextBoundaryUtc TEXT NOT NULL,
 BoundaryKind TEXT NOT NULL CHECK(COALESCE((COALESCE((BoundaryKind IN ('PLAY_CHECKPOINT','GAME_FINAL')),0)),0)),
 FormatKey TEXT NOT NULL, FormatVersion TEXT NOT NULL,
 ProviderKey TEXT NOT NULL, ProviderVersion TEXT NOT NULL,
 InputBindingsHash TEXT NOT NULL, ContentHash TEXT NOT NULL, CheckpointData BLOB,
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('READY','CONSUMED')),0)),0)),
 PRIMARY KEY(WorldID,GameID), UNIQUE(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,GameID) REFERENCES RunningGameEvents(WorldID,GameID),
 CHECK(COALESCE((COALESCE((State<>'READY' OR CheckpointData IS NOT NULL),0)),0)),
 CHECK(COALESCE((COALESCE((NextBoundaryUtc IS NULL OR (length(NextBoundaryUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',NextBoundaryUtc)=NextBoundaryUtc)),0)),0))
);
CREATE TABLE VenueChangeCommands (
 CommandID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL,
 CommandKey TEXT NOT NULL, RequestFingerprint TEXT NOT NULL,
 WorldUserID INTEGER NOT NULL, WorldOrganizationID INTEGER NOT NULL,
 TeamSeasonID INTEGER NOT NULL, WorldVenueID INTEGER NOT NULL,
 OccurredUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL CHECK(COALESCE((COALESCE((DataRevision>0),0)),0)),
 UNIQUE(WorldID,CommandKey), UNIQUE(WorldID,WorldVenueID,CommandID),
 FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 FOREIGN KEY(WorldID,WorldOrganizationID) REFERENCES WorldOrganizations(WorldID,WorldOrganizationID),
 FOREIGN KEY(WorldOrganizationID,TeamSeasonID) REFERENCES TeamSeasons(WorldOrganizationID,TeamSeasonID),
 FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID),
 CHECK(COALESCE((COALESCE((OccurredUtc IS NULL OR (length(OccurredUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OccurredUtc)=OccurredUtc)),0)),0))
);
CREATE TABLE WorldTimingPolicies (
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID), PolicyKey TEXT NOT NULL, PolicyVersion INTEGER NOT NULL CHECK(COALESCE((COALESCE((PolicyVersion>0),0)),0)),
 DurationSeconds INTEGER NOT NULL CHECK(COALESCE((COALESCE((DurationSeconds>0),0)),0)),
 PRIMARY KEY(WorldID,PolicyKey,PolicyVersion), UNIQUE(WorldID,PolicyKey,PolicyVersion,DurationSeconds)
);
CREATE TABLE VenueChangePlans (
 PlanID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, WorldVenueID INTEGER NOT NULL,
 FieldCode TEXT NOT NULL CHECK(COALESCE((COALESCE((FieldCode IN ('NAME','SURFACE')),0)),0)),
 InitialCommandID INTEGER NOT NULL,
 OriginalAnnouncedUtc TEXT NOT NULL, DueUtc TEXT NOT NULL,
 TimingPolicyKey TEXT NOT NULL, TimingPolicyVersion INTEGER NOT NULL,
 DurationSeconds INTEGER NOT NULL CHECK(COALESCE((COALESCE((DurationSeconds>0),0)),0)),
 UNIQUE(WorldID,WorldVenueID,FieldCode,PlanID),
 FOREIGN KEY(WorldID,WorldVenueID,InitialCommandID) REFERENCES VenueChangeCommands(WorldID,WorldVenueID,CommandID),
 FOREIGN KEY(WorldID,TimingPolicyKey,TimingPolicyVersion,DurationSeconds) REFERENCES WorldTimingPolicies(WorldID,PolicyKey,PolicyVersion,DurationSeconds),
 CHECK(COALESCE((COALESCE((COALESCE(length(OriginalAnnouncedUtc)=20 AND length(DueUtc)=20
 AND strftime('%Y-%m-%dT%H:%M:%SZ',OriginalAnnouncedUtc)=OriginalAnnouncedUtc
 AND strftime('%Y-%m-%dT%H:%M:%SZ',DueUtc)=DueUtc
 AND unixepoch(DueUtc)-unixepoch(OriginalAnnouncedUtc)=DurationSeconds,0)),0)),0)),
 CHECK(COALESCE((COALESCE((OriginalAnnouncedUtc IS NULL OR (length(OriginalAnnouncedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OriginalAnnouncedUtc)=OriginalAnnouncedUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((DueUtc IS NULL OR (length(DueUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',DueUtc)=DueUtc)),0)),0))
);
CREATE TABLE VenueChangeEvents (
 VenueChangeEventID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, WorldVenueID INTEGER NOT NULL,
 FieldCode TEXT NOT NULL CHECK(COALESCE((COALESCE((FieldCode IN ('NAME','SURFACE')),0)),0)),
 PlanID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 DesiredValue TEXT, CommandID INTEGER NOT NULL, SupersedesEventID INTEGER,
 AnnouncedUtc TEXT NOT NULL, AnnouncedRevision INTEGER NOT NULL CHECK(COALESCE((COALESCE((AnnouncedRevision>0),0)),0)),
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('PENDING','APPLIED','CANCELED','SUPERSEDED')),0)),0)),
 ResolvedUtc TEXT, ResolvedRevision INTEGER, ResolutionReason TEXT, ResolutionCommandID INTEGER,
 AppliedBeforeValue TEXT,
 UNIQUE(WorldID,VenueChangeEventID), UNIQUE(WorldID,ScheduledWorldEventID),
 UNIQUE(WorldID,WorldVenueID,FieldCode,VenueChangeEventID),
 FOREIGN KEY(WorldID,WorldVenueID,FieldCode,PlanID) REFERENCES VenueChangePlans(WorldID,WorldVenueID,FieldCode,PlanID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,WorldVenueID,CommandID) REFERENCES VenueChangeCommands(WorldID,WorldVenueID,CommandID),
 FOREIGN KEY(WorldID,WorldVenueID,ResolutionCommandID) REFERENCES VenueChangeCommands(WorldID,WorldVenueID,CommandID),
 FOREIGN KEY(WorldID,WorldVenueID,FieldCode,SupersedesEventID) REFERENCES VenueChangeEvents(WorldID,WorldVenueID,FieldCode,VenueChangeEventID),
 CHECK(COALESCE((COALESCE((FieldCode<>'NAME' OR (DesiredValue IS NOT NULL AND length(trim(DesiredValue))>0)),0)),0)),
 CHECK(COALESCE((COALESCE((COALESCE((State='PENDING' AND ResolvedUtc IS NULL AND ResolvedRevision IS NULL AND ResolutionReason IS NULL AND ResolutionCommandID IS NULL AND AppliedBeforeValue IS NULL)
 OR (State='APPLIED' AND ResolvedUtc>=AnnouncedUtc AND ResolvedRevision>=AnnouncedRevision AND ResolutionReason='DUE_TIME' AND ResolutionCommandID IS NULL)
 OR (State='CANCELED' AND ResolvedUtc>=AnnouncedUtc AND ResolvedRevision>=AnnouncedRevision AND AppliedBeforeValue IS NULL
 AND ((ResolutionReason='USER_CANCEL' AND ResolutionCommandID IS NOT NULL) OR (ResolutionReason='GOD_OVERRIDE' AND ResolutionCommandID IS NULL)))
 OR (State='SUPERSEDED' AND ResolvedUtc>=AnnouncedUtc AND ResolvedRevision>=AnnouncedRevision AND ResolutionReason='REPLACED' AND ResolutionCommandID IS NOT NULL AND AppliedBeforeValue IS NULL),0)),0)),0)),
 CHECK(COALESCE((COALESCE((AnnouncedUtc IS NULL OR (length(AnnouncedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',AnnouncedUtc)=AnnouncedUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((ResolvedUtc IS NULL OR (length(ResolvedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',ResolvedUtc)=ResolvedUtc)),0)),0))
);
CREATE UNIQUE INDEX WS_OnePendingField ON VenueChangeEvents(WorldID,WorldVenueID,FieldCode) WHERE State='PENDING';
CREATE TABLE WorldAdvanceRequests (
 AdvanceRequestID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL,
 StartedByUserID INTEGER NOT NULL, LastAdvancedByUserID INTEGER NOT NULL,
 CommandKey TEXT NOT NULL, RequestFingerprint TEXT NOT NULL,
 Mode TEXT NOT NULL CHECK(COALESCE((COALESCE((Mode IN ('NEXT_EVENT','TO_TIME')),0)),0)),
 TargetUtc TEXT, VisibleStopEventID INTEGER,
 OrderingPolicyKey TEXT NOT NULL CHECK(COALESCE((COALESCE((OrderingPolicyKey='WORLD_TIME_V1'),0)),0)),
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('RUNNING','PAUSED','BLOCKED','COMPLETED','CANCELED')),0)),0)),
 CursorUtc TEXT NOT NULL, CursorPhase INTEGER NOT NULL CHECK(COALESCE((COALESCE((CursorPhase BETWEEN 0 AND 3),0)),0)),
 PauseEventID INTEGER, StopReason TEXT,
 CreatedRevision INTEGER NOT NULL, ChangedRevision INTEGER NOT NULL,
 UNIQUE(WorldID,AdvanceRequestID), UNIQUE(WorldID,StartedByUserID,CommandKey),
 FOREIGN KEY(WorldID,StartedByUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 FOREIGN KEY(WorldID,LastAdvancedByUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 FOREIGN KEY(WorldID,VisibleStopEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,PauseEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 CHECK(COALESCE((COALESCE(((Mode='TO_TIME' AND TargetUtc IS NOT NULL) OR (Mode='NEXT_EVENT' AND TargetUtc IS NULL)),0)),0)),
 CHECK(COALESCE((COALESCE((ChangedRevision>=CreatedRevision),0)),0)),
 CHECK(COALESCE((COALESCE((TargetUtc IS NULL OR (length(TargetUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',TargetUtc)=TargetUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((CursorUtc IS NULL OR (length(CursorUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',CursorUtc)=CursorUtc)),0)),0))
);
CREATE UNIQUE INDEX WS_OneActiveAdvance ON WorldAdvanceRequests(WorldID) WHERE State IN ('RUNNING','PAUSED','BLOCKED');
CREATE TABLE SchedulerDispatchReceipts (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 StepKey TEXT NOT NULL CHECK(COALESCE((COALESCE((StepKey IN ('START','APPLY','COMPLETE')),0)),0)),
 AdvanceRequestID INTEGER NOT NULL,
 ExecutionFingerprint TEXT NOT NULL,
 OccurredUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL,
 PRIMARY KEY(WorldID,ScheduledWorldEventID,StepKey),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,AdvanceRequestID) REFERENCES WorldAdvanceRequests(WorldID,AdvanceRequestID),
 CHECK(COALESCE((COALESCE((OccurredUtc IS NULL OR (length(OccurredUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OccurredUtc)=OccurredUtc)),0)),0))
);
CREATE TABLE WorldParticipationCommands (
 CommandID INTEGER PRIMARY KEY, WorldID INTEGER NOT )CLOCK_SQL";
    sql += R"CLOCK_SQL(NULL, WorldUserID INTEGER NOT NULL,
 CommandKey TEXT NOT NULL, RequestFingerprint TEXT NOT NULL, OccurredUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL,
 UNIQUE(WorldID,WorldUserID,CommandID), UNIQUE(WorldID,WorldUserID,CommandKey),
 FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 CHECK(COALESCE((COALESCE((OccurredUtc IS NULL OR (length(OccurredUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OccurredUtc)=OccurredUtc)),0)),0))
);
CREATE TABLE WorldEventParticipants (
 ParticipationID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 PersonID INTEGER NOT NULL, CharacterID INTEGER NOT NULL, WorldUserID INTEGER NOT NULL,
 ParticipationKey TEXT NOT NULL,
 Required INTEGER NOT NULL CHECK(COALESCE((COALESCE((Required IN (0,1)),0)),0)),
 OfferedStartUtc TEXT NOT NULL, OfferedEndUtc TEXT,
 EndMode TEXT NOT NULL CHECK(COALESCE((COALESCE((EndMode IN ('FIXED','DOMAIN')),0)),0)),
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('OFFERED','ATTENDING','DECLINED','DELEGATED','FINISHED','MISSED','CANCELED')),0)),0)),
 DecisionCommandID INTEGER, DecisionUtc TEXT, DecisionRevision INTEGER,
 DelegatePersonID INTEGER,
 UNIQUE(WorldID,ParticipationID), UNIQUE(WorldID,ScheduledWorldEventID,PersonID,ParticipationKey),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,CharacterID,PersonID,WorldUserID) REFERENCES Characters(WorldID,CharacterID,PersonID,WorldUserID),
 FOREIGN KEY(WorldID,DelegatePersonID) REFERENCES People(WorldID,PersonID),
 FOREIGN KEY(WorldID,WorldUserID,DecisionCommandID) REFERENCES WorldParticipationCommands(WorldID,WorldUserID,CommandID),
 CHECK(COALESCE((COALESCE((COALESCE((EndMode='FIXED' AND OfferedEndUtc>OfferedStartUtc) OR (EndMode='DOMAIN' AND OfferedEndUtc IS NULL),0)),0)),0)),
 CHECK(COALESCE((COALESCE((COALESCE((State='OFFERED' AND DecisionCommandID IS NULL AND DecisionUtc IS NULL AND DecisionRevision IS NULL)
 OR (State<>'OFFERED' AND DecisionCommandID IS NOT NULL AND DecisionUtc IS NOT NULL AND DecisionRevision IS NOT NULL),0)),0)),0)),
 CHECK(COALESCE((COALESCE(((State='DELEGATED' AND DelegatePersonID IS NOT NULL) OR (State<>'DELEGATED' AND DelegatePersonID IS NULL)),0)),0)),
 CHECK(COALESCE((COALESCE((OfferedStartUtc IS NULL OR (length(OfferedStartUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OfferedStartUtc)=OfferedStartUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((OfferedEndUtc IS NULL OR (length(OfferedEndUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OfferedEndUtc)=OfferedEndUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((DecisionUtc IS NULL OR (length(DecisionUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',DecisionUtc)=DecisionUtc)),0)),0))
);
CREATE TABLE WorldAttendanceReservations (
 WorldID INTEGER NOT NULL, ParticipationID INTEGER NOT NULL,
 PersonID INTEGER NOT NULL, StartsUtc TEXT NOT NULL, EndsUtc TEXT,
 State TEXT NOT NULL CHECK(COALESCE((COALESCE((State IN ('HELD','COMPLETED','RELEASED')),0)),0)),
 ReleasedUtc TEXT,
 PRIMARY KEY(WorldID,ParticipationID),
 FOREIGN KEY(WorldID,ParticipationID) REFERENCES WorldEventParticipants(WorldID,ParticipationID),
 FOREIGN KEY(WorldID,PersonID) REFERENCES People(WorldID,PersonID),
 CHECK(COALESCE((COALESCE((EndsUtc IS NULL OR EndsUtc>StartsUtc),0)),0)),
 CHECK(COALESCE((COALESCE(((State='HELD' AND ReleasedUtc IS NULL) OR (State<>'HELD' AND ReleasedUtc IS NOT NULL)),0)),0)),
 CHECK(COALESCE((COALESCE((StartsUtc IS NULL OR (length(StartsUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',StartsUtc)=StartsUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((EndsUtc IS NULL OR (length(EndsUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',EndsUtc)=EndsUtc)),0)),0)),
 CHECK(COALESCE((COALESCE((ReleasedUtc IS NULL OR (length(ReleasedUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',ReleasedUtc)=ReleasedUtc)),0)),0))
);
CREATE TABLE WorldEventFacts (
 FactID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, FactKey TEXT NOT NULL, FactKind TEXT NOT NULL,
 ScheduledWorldEventID INTEGER, VenueChangeEventID INTEGER, GameID INTEGER,
 SourceFactID INTEGER,
 OccurredUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL,
 Visibility TEXT NOT NULL CHECK(COALESCE((COALESCE((Visibility IN ('PUBLIC','RESTRICTED','INTERNAL')),0)),0)),
 UNIQUE(WorldID,FactID), UNIQUE(WorldID,FactKey),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,VenueChangeEventID) REFERENCES VenueChangeEvents(WorldID,VenueChangeEventID),
 FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID),
 FOREIGN KEY(WorldID,SourceFactID) REFERENCES WorldEventFacts(WorldID,FactID),
 CHECK(COALESCE((COALESCE(((VenueChangeEventID IS NOT NULL AND GameID IS NULL) OR (VenueChangeEventID IS NULL AND GameID IS NOT NULL) OR (VenueChangeEventID IS NULL AND GameID IS NULL AND ScheduledWorldEventID IS NOT NULL)),0)),0)),
 CHECK(COALESCE((COALESCE((OccurredUtc IS NULL OR (length(OccurredUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',OccurredUtc)=OccurredUtc)),0)),0))
);
CREATE TABLE WorldFactReaders (
 WorldID INTEGER NOT NULL, FactID INTEGER NOT NULL, WorldUserID INTEGER NOT NULL,
 KnownFromUtc TEXT NOT NULL,
 PRIMARY KEY(WorldID,FactID,WorldUserID),
 FOREIGN KEY(WorldID,FactID) REFERENCES WorldEventFacts(WorldID,FactID),
 FOREIGN KEY(WorldID,WorldUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 CHECK(COALESCE((COALESCE((KnownFromUtc IS NULL OR (length(KnownFromUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',KnownFromUtc)=KnownFromUtc)),0)),0))
);

CREATE TRIGGER WS_InsertDraft BEFORE INSERT ON ScheduledWorldEvents WHEN NEW.State<>'DRAFT'
BEGIN SELECT RAISE(ABORT,'create and bind draft occurrence before publication'); END;
CREATE TRIGGER WS_EventUpdate BEFORE UPDATE ON ScheduledWorldEvents
WHEN NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID OR NEW.WorldID<>OLD.WorldID OR NEW.EventKey<>OLD.EventKey
 OR NEW.KindKey<>OLD.KindKey OR NEW.StartsUtc<>OLD.StartsUtc OR NEW.EndMode<>OLD.EndMode OR NEW.EndsUtc IS NOT OLD.EndsUtc
 OR NEW.CreatedUtc<>OLD.CreatedUtc OR NEW.CreatedRevision<>OLD.CreatedRevision OR NEW.Visibility<>OLD.Visibility
 OR NEW.SupersedesEventID IS NOT OLD.SupersedesEventID
 OR NOT ((OLD.State='DRAFT' AND NEW.State='PENDING')
 OR (OLD.State='PENDING' AND NEW.State IN ('RUNNING','COMPLETED','CANCELED','SUPERSEDED'))
 OR (OLD.State='RUNNING' AND NEW.State='COMPLETED'))
BEGIN SELECT RAISE(ABORT,'immutable occurrence payload or invalid lifecycle transition'); END;
CREATE TRIGGER WS_NoEventDelete BEFORE DELETE ON ScheduledWorldEvents BEGIN SELECT RAISE(ABORT,'retain scheduled occurrence history'); END;
CREATE TRIGGER WS_KickoffBinding BEFORE INSERT ON GameKickoffOccurrences
WHEN NOT EXISTS(SELECT 1 FROM ScheduledWorldEvents e JOIN WorldScheduledGames g ON g.WorldScheduledGameID=NEW.WorldScheduledGameID
 JOIN CompetitionSeasons cs ON cs.CompetitionSeasonID=g.CompetitionSeasonID JOIN WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID
 WHERE e.WorldID=NEW.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND e.State='DRAFT'
 AND e.KindKey='GAME_KICKOFF' AND e.EndMode='DOMAIN' AND e.StartsUtc=g.ScheduledUtc AND g.CompetitionSeasonID=NEW.CompetitionSeasonID AND c.WorldID=NEW.WorldID)
BEGIN SELECT RAISE(ABORT,'kickoff binding requires exact scoped source fixture and time'); END;
CREATE TRIGGER WS_KickoffUpdate BEFORE UPDATE ON GameKickoffOccurrences
WHEN NEW.WorldID<>OLD.WorldID OR NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID OR NEW.CompetitionSeasonID<>OLD.CompetitionSeasonID
 OR NEW.WorldScheduledGameID<>OLD.WorldScheduledGameID OR OLD.IsCurrent<>1 OR NEW.IsCurrent<>0
 OR NOT EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=OLD.WorldID AND e.ScheduledWorldEventID=OLD.ScheduledWorldEventID AND e.State IN ('CANCELED','SUPERSEDED'))
BEGIN SELECT RAISE(ABORT,'replace a terminal kickoff binding rather than editing it'); END;
CREATE TRIGGER WS_KickoffNoDelete BEFORE DELETE ON GameKickoffOccurrences BEGIN SELECT RAISE(ABORT,'retain kickoff schedule history'); END;
CREATE TRIGGER WS_VenueInsert BEFORE INSERT ON VenueChangeEvents
WHEN NEW.State<>'PENDING' OR NOT EXISTS(SELECT 1 FROM ScheduledWorldEvents e JOIN VenueChangePlans p ON p.WorldID=e.WorldID AND p.PlanID=NEW.PlanID
 JOIN VenueChangeCommands c ON c.WorldID=e.WorldID AND c.CommandID=NEW.CommandID
 WHERE e.WorldID=NEW.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND e.KindKey='VENUE_APPLY' AND e.State='DRAFT'
 AND e.EndMode='INSTANT' AND e.StartsUtc=p.DueUtc AND c.OccurredUtc=NEW.AnnouncedUtc
 AND p.OriginalAnnouncedUtc<=NEW.AnnouncedUtc AND NEW.AnnouncedUtc<p.DueUtc)
 OR (NEW.SupersedesEventID IS NOT NULL AND NOT EXISTS(SELECT 1 FROM VenueChangeEvents old WHERE old.WorldID=NEW.WorldID
 AND old.VenueChangeEventID=NEW.SupersedesEventID AND old.State='SUPERSEDED' AND old.PlanID=NEW.PlanID AND old.ResolutionCommandID=NEW.CommandID))
BEGIN SELECT RAISE(ABORT,'venue request must bind dated plan and valid supersession'); END;
CREATE TRIGGER WS_Publish BEFORE UPDATE OF State ON ScheduledWorldEvents
WHEN NEW.State='PENDING' AND NOT ((NEW.KindKey='GAME_KICKOFF' AND EXISTS(SELECT 1 FROM GameKickoffOccurrences g WHERE g.WorldID=NEW.WorldID AND g.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND g.IsCurrent=1))
 OR (NEW.KindKey IN ('SEASON_EVENT','TRADE_EVENT','VENUE_CEREMONY') AND EXISTS(SELECT 1 FROM SeasonEventInstances x WHERE x.WorldID=NEW.WorldID AND x.ScheduledWorldEventID=NEW.ScheduledWorldEventID))
 OR (NEW.KindKey='VENUE_APPLY' AND EXISTS(SELECT 1 FROM VenueChangeEvents v JOIN VenueChangePlans p ON p.WorldID=v.WorldID AND p.PlanID=v.PlanID
 WHERE v.WorldID=NEW.WorldID AND v.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND v.State='PENDING' AND p.DueUtc=NEW.StartsUtc)))
BEGIN SELECT RAISE(ABORT,'unbound occurrences cannot enter scheduler'); END;
CREATE TRIGGER WS_VenueUpdate BEFORE UPDATE ON VenueChangeEvents
WHEN NEW.VenueChangeEventID<>OLD.VenueChangeEventID OR NEW.WorldID<>OLD.WorldID OR NEW.WorldVenueID<>OLD.WorldVenueID
 OR NEW.FieldCode<>OLD.FieldCode OR NEW.PlanID<>OLD.PlanID OR NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID
 OR NEW.DesiredValue IS NOT OLD.DesiredValue OR NEW.CommandID<>OLD.CommandID OR NEW.SupersedesEventID IS NOT OLD.SupersedesEventID
 OR NEW.AnnouncedUtc<>OLD.AnnouncedUtc OR NEW.AnnouncedRevision<>OLD.AnnouncedRevision OR OLD.State<>'PENDING' OR NEW.State='PENDING'
BEGIN SELECT RAISE(ABORT,'venue payload and terminal state are immutable'); END;
CREATE TRIGGER WS_VenueResolve BEFORE UPDATE ON VenueChangeEvents
WHEN NOT EXISTS(SELECT 1 FROM Worlds w JOIN VenueChangePlans p ON p.WorldID=w.WorldID AND p.PlanID=NEW.PlanID
 JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 WHERE w.WorldID=NEW.WorldID AND NEW.ResolvedUtc=w.CurrentTimeUtc AND
 ((NEW.State='APPLIED' AND NEW.ResolutionReason='DUE_TIME' AND w.CurrentTimeUtc=p.DueUtc AND e.State='PENDING'
   AND (NEW.FieldCode<>'NAME' OR NEW.AppliedBeforeValue IS NOT NULL))
 OR (NEW.State='CANCELED' AND NEW.ResolutionReason='USER_CANCEL' AND w.CurrentTimeUtc<p.DueUtc AND e.State='CANCELED' AND e.ResolutionReason='USER_CANCEL')
 OR (NEW.State='SUPERSEDED' AND w.CurrentTimeUtc<p.DueUtc AND e.State='SUPERSEDED')
 OR (NEW.State='CANCELED' AND NEW.ResolutionReason='GOD_OVERRIDE' AND e.State='CANCELED' AND e.ResolutionReason='GOD_OVERRIDE')))
BEGIN SELECT RAISE(ABORT,'venue resolution must match simulation deadline and occurrence'); END;
CREATE TRIGGER WS_VenueNoDelete BEFORE DELETE ON VenueChangeEvents BEGIN SELECT RAISE(ABORT,'retain venue request history'); END;
CREATE TRIGGER WS_StartGame BEFORE UPDATE OF State ON ScheduledWorldEvents
WHEN NEW.State='RUNNING' AND (NEW.KindKey<>'GAME_KICKOFF' OR NOT EXISTS(SELECT 1 FROM RunningGameEvents r JOIN Games g ON g.WorldID=r.WorldID AND g.GameID=r.GameID
 JOIN GameKickoffOccurrences k ON k.WorldID=r.WorldID AND k.ScheduledWorldEventID=r.ScheduledWorldEventID
 JOIN GameVenueEvidence v ON v.WorldID=g.WorldID AND v.GameID=g.GameID JOIN Worlds w )CLOCK_SQL";
    sql += R"CLOCK_SQL(ON w.WorldID=g.WorldID
 WHERE r.WorldID=NEW.WorldID AND r.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND g.WorldScheduledGameID=k.WorldScheduledGameID
 AND g.GameStatus='IN_PROGRESS' AND g.StartedUtc=NEW.StartsUtc AND v.CapturedUtc=NEW.StartsUtc AND w.CurrentTimeUtc=NEW.StartsUtc)
 OR EXISTS(SELECT 1 FROM ScheduledWorldEvents due JOIN WorldEventKinds kind ON kind.WorldID=due.WorldID AND kind.KindKey=due.KindKey
 WHERE due.WorldID=NEW.WorldID AND due.State='PENDING' AND kind.PhaseCode=1 AND due.StartsUtc<=NEW.StartsUtc))
BEGIN SELECT RAISE(ABORT,'game start requires current evidence and completed automatic phase'); END;
CREATE TRIGGER WS_CompleteEvent BEFORE UPDATE OF State ON ScheduledWorldEvents
WHEN NEW.State='COMPLETED' AND NOT EXISTS(SELECT 1 FROM Worlds w WHERE w.WorldID=NEW.WorldID AND w.CurrentTimeUtc=NEW.ResolvedUtc AND
 ((NEW.KindKey='VENUE_APPLY' AND EXISTS(SELECT 1 FROM VenueChangeEvents v WHERE v.WorldID=NEW.WorldID AND v.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND v.State='APPLIED' AND v.ResolvedUtc=NEW.ResolvedUtc))
 OR (NEW.KindKey='GAME_KICKOFF' AND OLD.State='RUNNING' AND EXISTS(SELECT 1 FROM RunningGameEvents r JOIN FinalizedLeagueGameResults f ON f.WorldID=r.WorldID AND f.GameID=r.GameID
 WHERE r.WorldID=NEW.WorldID AND r.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND f.CompletedUtc=NEW.ResolvedUtc))))
BEGIN SELECT RAISE(ABORT,'completed occurrence requires committed owning-domain outcome'); END;
CREATE TRIGGER WS_Receipt BEFORE INSERT ON SchedulerDispatchReceipts
WHEN NOT EXISTS(SELECT 1 FROM ScheduledWorldEvents e JOIN Worlds w ON w.WorldID=e.WorldID JOIN WorldAdvanceRequests a ON a.WorldID=e.WorldID AND a.AdvanceRequestID=NEW.AdvanceRequestID WHERE e.WorldID=NEW.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 AND w.CurrentTimeUtc=NEW.OccurredUtc AND a.State='RUNNING' AND a.CursorUtc=NEW.OccurredUtc AND ((NEW.StepKey='COMPLETE' AND a.CursorPhase=0) OR (NEW.StepKey='APPLY' AND a.CursorPhase=1) OR (NEW.StepKey='START' AND a.CursorPhase=3)) AND ((NEW.StepKey='START' AND e.KindKey='GAME_KICKOFF' AND e.State='RUNNING')
 OR (NEW.StepKey='APPLY' AND e.KindKey='VENUE_APPLY' AND e.State='COMPLETED') OR (NEW.StepKey='COMPLETE' AND e.KindKey='GAME_KICKOFF' AND e.State='COMPLETED')))
BEGIN SELECT RAISE(ABORT,'dispatch receipt requires actual same-time domain step'); END;
CREATE TRIGGER WS_ReservationInsert BEFORE INSERT ON WorldAttendanceReservations
WHEN NOT EXISTS(SELECT 1 FROM WorldEventParticipants p WHERE p.WorldID=NEW.WorldID AND p.ParticipationID=NEW.ParticipationID
 AND p.PersonID=NEW.PersonID AND p.State='ATTENDING' AND p.OfferedStartUtc=NEW.StartsUtc AND p.OfferedEndUtc IS NEW.EndsUtc)
 OR EXISTS(SELECT 1 FROM WorldAttendanceReservations a WHERE a.WorldID=NEW.WorldID AND a.PersonID=NEW.PersonID AND a.State='HELD'
 AND a.StartsUtc<COALESCE(NEW.EndsUtc,'9999-12-31T23:59:59Z') AND NEW.StartsUtc<COALESCE(a.EndsUtc,'9999-12-31T23:59:59Z'))
BEGIN SELECT RAISE(ABORT,'attendance must match choice and cannot overlap for the same person'); END;
CREATE TRIGGER WS_ReservationUpdate BEFORE UPDATE ON WorldAttendanceReservations
WHEN NEW.WorldID<>OLD.WorldID OR NEW.ParticipationID<>OLD.ParticipationID OR NEW.PersonID<>OLD.PersonID OR NEW.StartsUtc<>OLD.StartsUtc
 OR OLD.State<>'HELD' OR NEW.State NOT IN ('RELEASED','COMPLETED') OR NEW.ReleasedUtc IS NULL
 OR (OLD.EndsUtc IS NOT NULL AND NEW.EndsUtc IS NOT OLD.EndsUtc)
 OR (OLD.EndsUtc IS NULL AND NEW.EndsUtc IS NOT NEW.ReleasedUtc)
BEGIN SELECT RAISE(ABORT,'release/complete reservation rather than editing its interval'); END;
CREATE TRIGGER WS_ParticipationUpdate BEFORE UPDATE ON WorldEventParticipants
WHEN NEW.ParticipationID<>OLD.ParticipationID OR NEW.WorldID<>OLD.WorldID OR NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID
 OR NEW.PersonID<>OLD.PersonID OR NEW.CharacterID<>OLD.CharacterID OR NEW.WorldUserID<>OLD.WorldUserID
 OR NEW.ParticipationKey<>OLD.ParticipationKey OR NEW.Required<>OLD.Required OR NEW.OfferedStartUtc<>OLD.OfferedStartUtc
 OR NEW.OfferedEndUtc IS NOT OLD.OfferedEndUtc OR NEW.EndMode<>OLD.EndMode
 OR NOT ((OLD.State='OFFERED' AND NEW.State IN ('ATTENDING','DECLINED','DELEGATED','MISSED','CANCELED'))
 OR (OLD.State='ATTENDING' AND NEW.State='FINISHED'))
BEGIN SELECT RAISE(ABORT,'invalid participation decision transition'); END;
CREATE TRIGGER WS_FactTime BEFORE INSERT ON WorldEventFacts
WHEN NOT EXISTS(SELECT 1 FROM Worlds w WHERE w.WorldID=NEW.WorldID AND w.CurrentTimeUtc=NEW.OccurredUtc)
BEGIN SELECT RAISE(ABORT,'outcome fact must use current simulation time'); END;
CREATE TRIGGER WS_OpenWeek BEFORE UPDATE OF State ON LeagueWeeks WHEN NEW.State='OPEN' AND OLD.State<>'FUTURE'
BEGIN SELECT RAISE(ABORT,'open only a future league week; venue deadlines are independent'); END;

CREATE TRIGGER WS_WorldTimingPolicies_NoUpdate BEFORE UPDATE ON WorldTimingPolicies BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_WorldTimingPolicies_NoDelete BEFORE DELETE ON WorldTimingPolicies BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_VenueChangePlans_NoUpdate BEFORE UPDATE ON VenueChangePlans BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_VenueChangePlans_NoDelete BEFORE DELETE ON VenueChangePlans BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_VenueChangeCommands_NoUpdate BEFORE UPDATE ON VenueChangeCommands BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_VenueChangeCommands_NoDelete BEFORE DELETE ON VenueChangeCommands BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_SchedulerDispatchReceipts_NoUpdate BEFORE UPDATE ON SchedulerDispatchReceipts BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_SchedulerDispatchReceipts_NoDelete BEFORE DELETE ON SchedulerDispatchReceipts BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_WorldEventFacts_NoUpdate BEFORE UPDATE ON WorldEventFacts BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_WorldEventFacts_NoDelete BEFORE DELETE ON WorldEventFacts BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_WorldFactReaders_NoUpdate BEFORE UPDATE ON WorldFactReaders BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_WorldFactReaders_NoDelete BEFORE DELETE ON WorldFactReaders BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_WorldEventReaders_NoUpdate BEFORE UPDATE ON WorldEventReaders BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_WorldEventReaders_NoDelete BEFORE DELETE ON WorldEventReaders BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_RunningGameEvents_NoUpdate BEFORE UPDATE ON RunningGameEvents BEGIN SELECT RAISE(ABORT,'immutable scheduling evidence'); END;
CREATE TRIGGER WS_RunningGameEvents_NoDelete BEFORE DELETE ON RunningGameEvents BEGIN SELECT RAISE(ABORT,'retain scheduling evidence'); END;

CREATE TRIGGER WS_AdvanceInsert BEFORE INSERT ON WorldAdvanceRequests
WHEN NEW.State<>'RUNNING' OR NOT EXISTS(SELECT 1 FROM WorldUsers u JOIN Worlds w ON w.WorldID=u.WorldID
 WHERE u.WorldID=NEW.WorldID AND u.WorldUserID=NEW.StartedByUserID AND u.Active=1 AND u.CanAdvanceWorld=1
 AND NEW.LastAdvancedByUserID=u.WorldUserID AND NEW.CursorUtc=w.CurrentTimeUtc AND (NEW.TargetUtc IS NULL OR NEW.TargetUtc>=w.CurrentTimeUtc))
BEGIN SELECT RAISE(ABORT,'advancement requires an authorized current-time request'); END;
CREATE TRIGGER WS_AdvanceUpdate BEFORE UPDATE ON WorldAdvanceRequests
WHEN NEW.AdvanceRequestID<>OLD.AdvanceRequestID OR NEW.WorldID<>OLD.WorldID OR NEW.StartedByUserID<>OLD.StartedByUserID
 OR NEW.CommandKey<>OLD.CommandKey OR NEW.RequestFingerprint<>OLD.RequestFingerprint OR NEW.Mode<>OLD.Mode
 OR NEW.TargetUtc IS NOT OLD.TargetUtc OR NEW.VisibleStopEventID IS NOT OLD.VisibleStopEventID OR NEW.OrderingPolicyKey<>OLD.OrderingPolicyKey
 OR NEW.CreatedRevision<>OLD.CreatedRevision OR OLD.State IN ('COMPLETED','CANCELED') OR NEW.CursorUtc<OLD.CursorUtc
 OR (NEW.CursorUtc=OLD.CursorUtc AND NEW.CursorPhase<OLD.CursorPhase) OR NEW.ChangedRevision<OLD.ChangedRevision
 OR NOT EXISTS(SELECT 1 FROM WorldUsers u WHERE u.WorldID=NEW.WorldID AND u.WorldUserID=NEW.LastAdvancedByUserID AND u.Active=1 AND u.CanAdvanceWorld=1)
BEGIN SELECT RAISE(ABORT,'immutable advance intent or invalid resume cursor'); END;
CREATE TRIGGER WS_ParticipationInsert BEFORE INSERT ON WorldEventParticipants
WHEN NEW.State<>'OFFERED' OR NOT EXISTS(SELECT 1 FROM Characters c JOIN ScheduledWorldEvents e ON e.WorldID=c.WorldID
 WHERE c.WorldID=NEW.WorldID AND c.CharacterID=NEW.CharacterID AND c.PersonID=NEW.PersonID AND c.WorldUserID=NEW.WorldUserID
 AND c.EndsUtc IS NULL AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND e.State IN ('PENDING','RUNNING'))
BEGIN SELECT RAISE(ABORT,'offer participation through an active controlled character'); END;
CREATE TRIGGER WS_ParticipationDecision BEFORE UPDATE ON WorldEventParticipants
WHEN (NEW.Required=1 AND NEW.State IN ('DECLINED','MISSED'))
 OR (OLD.State<>'OFFERED' AND (NEW.DecisionCommandID IS NOT OLD.DecisionCommandID
 OR NEW.DecisionUtc IS NOT OLD.DecisionUtc OR NEW.DecisionRevision IS NOT OLD.DecisionRevision OR NEW.DelegatePersonID IS NOT OLD.DelegatePersonID))
BEGIN SELECT RAISE(ABORT,'required participation needs supported delegation; retain choice evidence'); END;
CREATE TRIGGER WS_CheckpointInsert BEFORE INSERT ON WorldGameCheckpoints
WHEN NOT EXISTS(SELECT 1 FROM RunningGameEvents r JOIN ScheduledWorldEvents e ON e.WorldID=r.WorldID AND e.ScheduledWorldEventID=r.ScheduledWorldEventID
 JOIN Games g ON g.WorldID=r.WorldID AND g.GameID=r.GameID JOIN Worlds w ON w.WorldID=r.WorldID
 WHERE r.WorldID=NEW.WorldID AND r.GameID=NEW.GameID AND r.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 AND e.State='RUNNING' AND NEW.State='READY' AND NEW.NextBoundaryUtc>=w.CurrentTimeUtc
 AND NEW.ProviderKey=g.SimulationProviderKey AND NEW.ProviderVersion=g.SimulationProviderVersion)
BEGIN SELECT RAISE(ABORT,'checkpoint requires a compatible running game and future boundary'); END;
CREATE TRIGGER WS_CheckpointUpdate BEFORE UPDATE ON WorldGameCheckpoints
WHEN NEW.WorldID<>OLD.WorldID OR NEW.GameID<>OLD.GameID OR NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID
 OR OLD.State='CONSUMED' OR NEW.Generation<=OLD.Generation OR NEW.NextBoundaryUtc<OLD.NextBoundaryUtc
 OR NEW.ProviderKey<>OLD.ProviderKey OR NEW.ProviderVersion<>OLD.ProviderVersion OR NEW.InputBindingsHash<>OLD.InputBindingsHash
BEGIN SELECT RAISE(ABORT,'checkpoint generation must move forward for the same game provider'); END;
CREATE TRIGGER WS_EventKnowledge BEFORE INSERT ON WorldEventReaders
WHEN NOT EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=NEW.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND NEW.KnownFromUtc>=e.CreatedUtc)
BEGIN SELECT RAISE(ABORT,'event cannot be known before it was created'); END;
CREATE TRIGGER WS_FactKnowledge BEFORE INSERT ON WorldFactReaders
WHEN NOT EXISTS(SELECT 1 FROM WorldEventFacts f WHERE f.WorldID=NEW.WorldID AND f.FactID=NEW.FactID AND NEW.KnownFromUtc>=f.OccurredUtc)
BEGIN SELECT RAISE(ABORT,'fact cannot be known before it occurred'); END;
CREATE TRIGGER WS_VenueRoot BEFORE INSERT ON VenueChangeEvents
WHEN NOT EXISTS(SELECT 1 FROM VenueChangePlans p JOIN ScheduledWorldEvents e ON e.WorldID=p.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 WHERE p.WorldID=NEW.WorldID AND p.PlanID=NEW.PlanID AND
 ((NEW.SupersedesEventID IS NULL AND NEW.CommandID=p.InitialCommandID AND NEW.AnnouncedUtc=p.OriginalAnnouncedUtc AND e.SupersedesEventID IS NULL)
 OR (NEW.SupersedesEventID IS NOT NULL AND EXISTS(SELECT 1 FROM VenueChangeEvents old WHERE old.WorldID=NEW.WorldID
 AND old.VenueChangeEventID=NEW.SupersedesEventID AND old.PlanID=p.PlanID AND e.SupersedesEventID=old.ScheduledWor)CLOCK_SQL";
    sql += R"CLOCK_SQL(ldEventID))))
BEGIN SELECT RAISE(ABORT,'fresh announcement needs a fresh dated plan; revision retains its root'); END;

CREATE TRIGGER WS_WorldEventKinds_NoDelete BEFORE DELETE ON WorldEventKinds BEGIN SELECT RAISE(ABORT,'retain scheduler state/history'); END;

CREATE TRIGGER WS_WorldEventParticipants_NoDelete BEFORE DELETE ON WorldEventParticipants BEGIN SELECT RAISE(ABORT,'retain scheduler state/history'); END;

CREATE TRIGGER WS_WorldAttendanceReservations_NoDelete BEFORE DELETE ON WorldAttendanceReservations BEGIN SELECT RAISE(ABORT,'retain scheduler state/history'); END;

CREATE TRIGGER WS_WorldAdvanceRequests_NoDelete BEFORE DELETE ON WorldAdvanceRequests BEGIN SELECT RAISE(ABORT,'retain scheduler state/history'); END;

CREATE TRIGGER WS_WorldGameCheckpoints_NoDelete BEFORE DELETE ON WorldGameCheckpoints BEGIN SELECT RAISE(ABORT,'retain scheduler state/history'); END;

CREATE UNIQUE INDEX WS_OneActivePersonOffer ON WorldEventParticipants(WorldID,ScheduledWorldEventID,PersonID) WHERE State IN ('OFFERED','ATTENDING');
CREATE TRIGGER WS_NoClockSkip BEFORE UPDATE OF CurrentTimeUtc ON Worlds
WHEN NEW.CurrentTimeUtc<OLD.CurrentTimeUtc
 OR EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=NEW.WorldID AND e.State='PENDING' AND e.StartsUtc<NEW.CurrentTimeUtc)
 OR EXISTS(SELECT 1 FROM WorldGameCheckpoints c WHERE c.WorldID=NEW.WorldID AND c.State='READY' AND c.NextBoundaryUtc<NEW.CurrentTimeUtc)
 OR (NEW.CurrentTimeUtc>OLD.CurrentTimeUtc AND EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=NEW.WorldID AND e.State='RUNNING'
 AND NOT EXISTS(SELECT 1 FROM WorldGameCheckpoints c WHERE c.WorldID=e.WorldID AND c.ScheduledWorldEventID=e.ScheduledWorldEventID AND c.State='READY')))
BEGIN SELECT RAISE(ABORT,'clock cannot rewind or skip pending work/checkpoints'); END;
CREATE TRIGGER WS_ParticipationCommand_NoUpdate BEFORE UPDATE ON WorldParticipationCommands BEGIN SELECT RAISE(ABORT,'immutable participation command'); END;
CREATE TRIGGER WS_ParticipationCommand_NoDelete BEFORE DELETE ON WorldParticipationCommands BEGIN SELECT RAISE(ABORT,'retain participation command'); END;

CREATE TRIGGER WS_WorldEventKinds_NoUpdate BEFORE UPDATE ON WorldEventKinds BEGIN SELECT RAISE(ABORT,'handler contract binding is immutable'); END;

-- Replace World4 clock-equality guards, never omit replacements in production adoption.
DROP TRIGGER WS_NoClockSkip;
DROP TRIGGER WS_StartGame;
DROP TRIGGER WS_CompleteEvent;
DROP TRIGGER WS_Receipt;
DROP TRIGGER WS_CheckpointInsert;
DROP TRIGGER WS_CheckpointUpdate;
DROP TRIGGER WS_FactTime;
DROP TRIGGER WS_VenueResolve;

DROP TABLE WorldGameCheckpoints;
DROP TABLE SchedulerDispatchReceipts;
ALTER TABLE ScheduledWorldEvents ADD COLUMN DispatchPriority INTEGER NOT NULL DEFAULT 100 CHECK(COALESCE((DispatchPriority>=0),0));
CREATE TABLE GameClockProfiles (
 WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID), ProfileKey TEXT NOT NULL, ProfileVersion INTEGER NOT NULL CHECK(COALESCE((ProfileVersion>0),0)),
 RegulationPeriods INTEGER NOT NULL CHECK(COALESCE((RegulationPeriods=4),0)), QuarterSeconds INTEGER NOT NULL CHECK(COALESCE((QuarterSeconds>0),0)),
 OvertimePolicyKey TEXT NOT NULL, TimingPolicyKey TEXT NOT NULL, TimingPolicyVersion INTEGER NOT NULL CHECK(COALESCE((TimingPolicyVersion>0),0)),
 PolicyContentHash TEXT NOT NULL, PRIMARY KEY(WorldID,ProfileKey,ProfileVersion)
);
CREATE TABLE GameProcessingWindows (
 WindowID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID), CompetitionSeasonID INTEGER NOT NULL REFERENCES CompetitionSeasons(CompetitionSeasonID),
 WindowKey TEXT NOT NULL, KickoffUtc TEXT NOT NULL, MembershipHash TEXT NOT NULL, State TEXT NOT NULL CHECK(COALESCE((State IN ('DRAFT','ADMITTED','PAUSED','RESOLVED')),0)),
 CreatedByUserID INTEGER NOT NULL, CreatedRevision INTEGER NOT NULL CHECK(COALESCE((CreatedRevision>=0),0)), ResolvedRevision INTEGER,
 UNIQUE(WorldID,WindowID), UNIQUE(WorldID,WindowKey), FOREIGN KEY(WorldID,CreatedByUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 CHECK(COALESCE((COALESCE(length(KickoffUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',KickoffUtc)=KickoffUtc,0)),0)),
 CHECK(COALESCE(((State='RESOLVED' AND ResolvedRevision IS NOT NULL AND ResolvedRevision>=CreatedRevision) OR (State<>'RESOLVED' AND ResolvedRevision IS NULL)),0))
);
CREATE UNIQUE INDEX CW_OneWindow ON GameProcessingWindows(WorldID) WHERE State IN ('DRAFT','ADMITTED','PAUSED');
CREATE TABLE GameWindowMembers (
 WorldID INTEGER NOT NULL, WindowID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, WorldScheduledGameID INTEGER NOT NULL,
 PRIMARY KEY(WorldID,WindowID,WorldScheduledGameID), UNIQUE(WorldID,WorldScheduledGameID), UNIQUE(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,WindowID) REFERENCES GameProcessingWindows(WorldID,WindowID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES GameKickoffOccurrences(WorldID,ScheduledWorldEventID)
);
CREATE TABLE GameRuns (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, WindowID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 KickoffUtc TEXT NOT NULL, ProfileKey TEXT NOT NULL, ProfileVersion INTEGER NOT NULL,
 ProviderKey TEXT NOT NULL, ProviderVersion TEXT NOT NULL, InputBindingsHash TEXT NOT NULL,
 InputArtifactID INTEGER NOT NULL REFERENCES GameArtifacts(GameArtifactID), InitialVenueVersionID INTEGER,
 State TEXT NOT NULL CHECK(COALESCE((State IN ('PREPARED','RUNNING','PAUSED','FINAL')),0)),
 PRIMARY KEY(WorldID,GameID), UNIQUE(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,GameID) REFERENCES Games(WorldID,GameID), FOREIGN KEY(WorldID,WindowID) REFERENCES GameProcessingWindows(WorldID,WindowID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES GameWindowMembers(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ProfileKey,ProfileVersion) REFERENCES GameClockProfiles(WorldID,ProfileKey,ProfileVersion)
);
CREATE TABLE GamePeriods (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, PeriodNo INTEGER NOT NULL CHECK(COALESCE((PeriodNo>0),0)),
 PeriodKind TEXT NOT NULL CHECK(COALESCE((PeriodKind IN ('REGULATION','OVERTIME')),0)), ClockMode TEXT NOT NULL CHECK(COALESCE((ClockMode IN ('COUNTDOWN','UNTIMED')),0)),
 DurationSeconds INTEGER, PolicyKey TEXT NOT NULL, PRIMARY KEY(WorldID,GameID,PeriodNo),
 FOREIGN KEY(WorldID,GameID) REFERENCES GameRuns(WorldID,GameID),
 CHECK(COALESCE(((ClockMode='COUNTDOWN' AND DurationSeconds>0) OR (ClockMode='UNTIMED' AND DurationSeconds IS NULL)),0)),
 CHECK(COALESCE(((PeriodKind='REGULATION' AND PeriodNo BETWEEN 1 AND 4 AND ClockMode='COUNTDOWN') OR (PeriodKind='OVERTIME' AND PeriodNo>4)),0))
);
CREATE TABLE GameExecutionClaims (
 ClaimID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, CommandKey TEXT NOT NULL,
 ExpectedGeneration INTEGER NOT NULL CHECK(COALESCE((ExpectedGeneration>=-1),0)), TargetElapsedSeconds INTEGER NOT NULL CHECK(COALESCE((TargetElapsedSeconds>=0),0)),
 ProviderStepHash TEXT NOT NULL, InputBindingsHash TEXT NOT NULL,
 State TEXT NOT NULL CHECK(COALESCE((State IN ('OPEN','COMMITTED','CANCELED')),0)), UNIQUE(WorldID,ClaimID), UNIQUE(WorldID,GameID,CommandKey),
 FOREIGN KEY(WorldID,GameID) REFERENCES GameRuns(WorldID,GameID)
);
CREATE UNIQUE INDEX CW_OneClaim ON GameExecutionClaims(WorldID,GameID) WHERE State='OPEN';
CREATE TABLE GameExecutionCheckpoints (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, Generation INTEGER NOT NULL CHECK(COALESCE((Generation>=0),0)), ClaimID INTEGER NOT NULL,
 PeriodNo INTEGER NOT NULL, RemainingSeconds INTEGER, ElapsedSeconds INTEGER NOT NULL CHECK(COALESCE((ElapsedSeconds>=0),0)),
 State TEXT NOT NULL CHECK(COALESCE((State IN ('RUNNING','PAUSED','FINAL')),0)), FormatKey TEXT NOT NULL, FormatVersion INTEGER NOT NULL CHECK(COALESCE((FormatVersion>0),0)),
 ContentHash TEXT NOT NULL, ResumeData BLOB NOT NULL, PRIMARY KEY(WorldID,GameID,Generation), UNIQUE(WorldID,ClaimID),
 FOREIGN KEY(WorldID,GameID,PeriodNo) REFERENCES GamePeriods(WorldID,GameID,PeriodNo),
 FOREIGN KEY(WorldID,ClaimID) REFERENCES GameExecutionClaims(WorldID,ClaimID)
);
CREATE TABLE GameTimingIntervals (
 IntervalID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, ClaimID INTEGER NOT NULL,
 StartsElapsedSeconds INTEGER NOT NULL CHECK(COALESCE((StartsElapsedSeconds>=0),0)), EndsElapsedSeconds INTEGER NOT NULL,
 PeriodNo INTEGER NOT NULL, ClockBeforeSeconds INTEGER, ClockAfterSeconds INTEGER,
 Certainty TEXT NOT NULL CHECK(COALESCE((Certainty IN ('SIMULATED','EXACT_SOURCE','ESTIMATED')),0)), SourceClockText TEXT, SourceRef TEXT,
 UNIQUE(WorldID,IntervalID), FOREIGN KEY(WorldID,GameID,PeriodNo) REFERENCES GamePeriods(WorldID,GameID,PeriodNo),
 FOREIGN KEY(WorldID,ClaimID) REFERENCES GameExecutionClaims(WorldID,ClaimID),
 CHECK(COALESCE((EndsElapsedSeconds>StartsElapsedSeconds),0)), CHECK(COALESCE((ClockAfterSeconds IS NULL OR (ClockAfterSeconds>=0 AND ClockAfterSeconds<=ClockBeforeSeconds)),0))
);
CREATE INDEX CW_Intervals ON GameTimingIntervals(WorldID,GameID,StartsElapsedSeconds,EndsElapsedSeconds);
CREATE TABLE GameTimingReasons (
 WorldID INTEGER NOT NULL, IntervalID INTEGER NOT NULL, ReasonKey TEXT NOT NULL,
 PRIMARY KEY(WorldID,IntervalID,ReasonKey), FOREIGN KEY(WorldID,IntervalID) REFERENCES GameTimingIntervals(WorldID,IntervalID)
);
CREATE TABLE GamePlayTiming (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, PlayOrdinal INTEGER NOT NULL CHECK(COALESCE((PlayOrdinal>0),0)),
 CanonicalPlayKey TEXT NOT NULL, PeriodNo INTEGER NOT NULL, RemainingSeconds INTEGER,
 StartsElapsedSeconds INTEGER NOT NULL CHECK(COALESCE((StartsElapsedSeconds>=0),0)), EndsElapsedSeconds INTEGER NOT NULL,
 Certainty TEXT NOT NULL CHECK(COALESCE((Certainty IN ('SIMULATED','EXACT_SOURCE','ESTIMATED')),0)), SourceClockText TEXT, SourceRef TEXT,
 PRIMARY KEY(WorldID,GameID,PlayOrdinal), UNIQUE(WorldID,GameID,CanonicalPlayKey),
 FOREIGN KEY(WorldID,GameID,PeriodNo) REFERENCES GamePeriods(WorldID,GameID,PeriodNo), CHECK(COALESCE((EndsElapsedSeconds>=StartsElapsedSeconds),0))
);
CREATE TABLE GameFinalSaves (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, Generation INTEGER NOT NULL, FinalElapsedSeconds INTEGER NOT NULL,
 CompletedUtc TEXT NOT NULL, CommandKey TEXT NOT NULL, ResultHash TEXT NOT NULL, ArtifactManifestHash TEXT NOT NULL,
 CommittedRevision INTEGER NOT NULL, PRIMARY KEY(WorldID,GameID), UNIQUE(WorldID,CommandKey),
 FOREIGN KEY(WorldID,GameID,Generation) REFERENCES GameExecutionCheckpoints(WorldID,GameID,Generation)
);
CREATE TABLE WorldExecutionContexts (
 ContextID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL REFERENCES Worlds(WorldID), ActorUserID INTEGER NOT NULL,
 DriverKind TEXT NOT NULL CHECK(COALESCE((DriverKind IN ('WORLD','GAME','ADMIN','EDIT')),0)), GameID INTEGER, ClaimID INTEGER, AdvanceRequestID INTEGER,
 EffectiveUtc TEXT NOT NULL, WorldAnchorUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL CHECK(COALESCE((DataRevision>0),0)),
 PhaseCode INTEGER NOT NULL CHECK(COALESCE((PhaseCode BETWEEN 0 AND 3),0)), CommandKey TEXT NOT NULL,
 UNIQUE(WorldID,ContextID), UNIQUE(WorldID,CommandKey), FOREIGN KEY(WorldID,ActorUserID) REFERENCES WorldUsers(WorldID,WorldUserID),
 FOREIGN KEY(WorldID,GameID) REFERENCES GameRuns(WorldID,GameID), FOREIGN KEY(WorldID,ClaimID) REFERENCES GameExecutionClaims(WorldID,ClaimID),
 FOREIGN KEY(WorldID,AdvanceRequestID) REFERENCES WorldAdvanceRequests(WorldID,AdvanceRequestID),
 CHECK(COALESCE(((DriverKind='GAME' AND GameID IS NOT NULL AND ClaimID IS NOT NULL AND AdvanceRequestID IS NULL)
 OR (DriverKind='WORLD' AND GameID IS NULL AND ClaimID IS NULL AND AdvanceRequestID IS NOT NULL)
 OR (DriverKind IN ('ADMIN','EDIT') AND GameID IS NULL AND ClaimID IS NULL AND AdvanceRequestID IS NULL)),0)),
 CHECK(COALESCE((COALESCE(length(EffectiveUtc)=20 AND strftime('%Y-%m-%dT%H:%M:%SZ',EffectiveUtc)=EffectiveUtc,0)),0))
);
CREATE TABLE SchedulerDispatchReceipts (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, StepKey TEXT NOT NULL CHECK(COALESCE((StepKey IN ('START','APPLY','COMPLETE','RESOLVE')),0)),
 )CLOCK_SQL";
    sql += R"CLOCK_SQL(ContextID INTEGER NOT NULL, ExecutionFingerprint TEXT NOT NULL, OccurredUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL,
 PRIMARY KEY(WorldID,ScheduledWorldEventID,StepKey), FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ContextID) REFERENCES WorldExecutionContexts(WorldID,ContextID)
);
ALTER TABLE VenueChangeEvents ADD COLUMN ResolutionContextID INTEGER REFERENCES WorldExecutionContexts(ContextID);
ALTER TABLE WorldEventFacts ADD COLUMN ExecutionContextID INTEGER REFERENCES WorldExecutionContexts(ContextID);
ALTER TABLE GameVenueEvidence ADD COLUMN CaptureContextID INTEGER REFERENCES WorldExecutionContexts(ContextID);
ALTER TABLE FinalizedLeagueGameResults ADD COLUMN ExecutionContextID INTEGER REFERENCES WorldExecutionContexts(ContextID);
CREATE TABLE TemporalVenueVersions (
 VersionID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, WorldVenueID INTEGER NOT NULL, EffectiveUtc TEXT NOT NULL,
 DataRevision INTEGER NOT NULL, VenueName TEXT NOT NULL CHECK(COALESCE((length(trim(VenueName))>0),0)), TurfType TEXT,
 SourceEventID INTEGER, ContextID INTEGER, StateHash TEXT NOT NULL,
 UNIQUE(WorldID,VersionID), UNIQUE(WorldID,WorldVenueID,EffectiveUtc,DataRevision),
 FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID),
 FOREIGN KEY(WorldID,SourceEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ContextID) REFERENCES WorldExecutionContexts(WorldID,ContextID),
 CHECK(COALESCE((SourceEventID IS NULL OR ContextID IS NOT NULL),0))
);
CREATE INDEX CW_VenueAsOf ON TemporalVenueVersions(WorldID,WorldVenueID,EffectiveUtc DESC,DataRevision DESC);
CREATE TABLE SeasonEventDefinitions (
 DefinitionID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, CompetitionSeasonID INTEGER NOT NULL REFERENCES CompetitionSeasons(CompetitionSeasonID),
 EventKey TEXT NOT NULL, EventType TEXT NOT NULL, TriggerKind TEXT NOT NULL CHECK(COALESCE((TriggerKind IN ('FIXED','TEAM_HOME_LOCAL','AFTER_CHAMPIONSHIP_FINAL')),0)),
 LocalDateTime TEXT, SourceZone TEXT, FixedUtc TEXT, SourceFileHash TEXT NOT NULL, SourceLine INTEGER NOT NULL CHECK(COALESCE((SourceLine>0),0)),
 Priority INTEGER NOT NULL CHECK(COALESCE((Priority>=0),0)), PolicyKey TEXT NOT NULL, PolicyVersion INTEGER NOT NULL CHECK(COALESCE((PolicyVersion>0),0)),
 InitializationMode TEXT NOT NULL CHECK(COALESCE((InitializationMode IN ('PENDING','HISTORICAL_REFERENCE','ADOPTED_FROM_CORE')),0)),
 UNIQUE(WorldID,DefinitionID), UNIQUE(WorldID,CompetitionSeasonID,EventKey),
 FOREIGN KEY(WorldID) REFERENCES Worlds(WorldID),
 CHECK(COALESCE(((TriggerKind='FIXED' AND FixedUtc IS NOT NULL AND LocalDateTime IS NOT NULL AND SourceZone IS NOT NULL)
 OR (TriggerKind='TEAM_HOME_LOCAL' AND FixedUtc IS NULL AND LocalDateTime IS NOT NULL AND SourceZone='TEAM_HOME_CITY')
 OR (TriggerKind='AFTER_CHAMPIONSHIP_FINAL' AND FixedUtc IS NULL AND LocalDateTime IS NULL)),0))
);
CREATE TABLE TeamHomeTimeZones (
 BindingID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, TeamSeasonID INTEGER NOT NULL REFERENCES TeamSeasons(TeamSeasonID),
 EffectiveUtc TEXT NOT NULL, IanaZoneID TEXT NOT NULL, TzdbVersion TEXT NOT NULL, SourceHash TEXT NOT NULL,
 UNIQUE(WorldID,BindingID), UNIQUE(WorldID,TeamSeasonID,EffectiveUtc), FOREIGN KEY(WorldID) REFERENCES Worlds(WorldID)
);
CREATE TABLE SeasonEventInstances (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, DefinitionID INTEGER NOT NULL,
 InstanceScopeKey TEXT NOT NULL, TeamSeasonID INTEGER REFERENCES TeamSeasons(TeamSeasonID), ZoneBindingID INTEGER,
 ParentGameID INTEGER, ResolvedZone TEXT, ConversionPolicyVersion TEXT,
 PRIMARY KEY(WorldID,ScheduledWorldEventID), UNIQUE(WorldID,DefinitionID,InstanceScopeKey),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,DefinitionID) REFERENCES SeasonEventDefinitions(WorldID,DefinitionID),
 FOREIGN KEY(WorldID,ZoneBindingID) REFERENCES TeamHomeTimeZones(WorldID,BindingID), FOREIGN KEY(WorldID,ParentGameID) REFERENCES GameFinalSaves(WorldID,GameID)
);
CREATE TABLE EventDependencies (
 WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL, RequiresEventID INTEGER NOT NULL,
 PRIMARY KEY(WorldID,ScheduledWorldEventID,RequiresEventID), CHECK(COALESCE((ScheduledWorldEventID<>RequiresEventID),0)),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,RequiresEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID)
);
CREATE TABLE EventOutcomes (
 OutcomeID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, ScheduledWorldEventID INTEGER NOT NULL,
 ContextID INTEGER NOT NULL, HistoricalUtc TEXT NOT NULL, HandlerKey TEXT NOT NULL, HandlerVersion INTEGER NOT NULL CHECK(COALESCE((HandlerVersion>0),0)),
 OutcomeHash TEXT NOT NULL, PolicyKey TEXT NOT NULL,
 UNIQUE(WorldID,OutcomeID), UNIQUE(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ScheduledWorldEventID) REFERENCES ScheduledWorldEvents(WorldID,ScheduledWorldEventID),
 FOREIGN KEY(WorldID,ContextID) REFERENCES WorldExecutionContexts(WorldID,ContextID)
);
CREATE TABLE SeasonTradingVersions (
 VersionID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, CompetitionSeasonID INTEGER NOT NULL REFERENCES CompetitionSeasons(CompetitionSeasonID),
 EffectiveUtc TEXT NOT NULL, DataRevision INTEGER NOT NULL, TradingOpen INTEGER NOT NULL CHECK(COALESCE((TradingOpen IN (0,1)),0)), OutcomeID INTEGER,
 UNIQUE(WorldID,VersionID), FOREIGN KEY(WorldID,OutcomeID) REFERENCES EventOutcomes(WorldID,OutcomeID)
);
CREATE INDEX CW_TradingAsOf ON SeasonTradingVersions(WorldID,CompetitionSeasonID,EffectiveUtc DESC,DataRevision DESC);
CREATE TABLE VenueCeremonyOutcomes (
 WorldID INTEGER NOT NULL, OutcomeID INTEGER NOT NULL, WorldVenueID INTEGER NOT NULL, CeremonyKey TEXT NOT NULL,
 PRIMARY KEY(WorldID,OutcomeID), FOREIGN KEY(WorldID,OutcomeID) REFERENCES EventOutcomes(WorldID,OutcomeID),
 FOREIGN KEY(WorldID,WorldVenueID) REFERENCES WorldVenues(WorldID,WorldVenueID)
);
CREATE TABLE TemporalEffectReceipts (
 WorldID INTEGER NOT NULL, OutcomeID INTEGER NOT NULL, EffectKey TEXT NOT NULL,
 VenueVersionID INTEGER, TradingVersionID INTEGER,
 PRIMARY KEY(WorldID,OutcomeID,EffectKey), FOREIGN KEY(WorldID,OutcomeID) REFERENCES EventOutcomes(WorldID,OutcomeID),
 FOREIGN KEY(WorldID,VenueVersionID) REFERENCES TemporalVenueVersions(WorldID,VersionID),
 FOREIGN KEY(WorldID,TradingVersionID) REFERENCES SeasonTradingVersions(WorldID,VersionID),
 CHECK(COALESCE(((VenueVersionID IS NOT NULL AND TradingVersionID IS NULL) OR (VenueVersionID IS NULL AND TradingVersionID IS NOT NULL)),0))
);
CREATE TABLE CausalReadFences (
 WorldID INTEGER NOT NULL, ContextID INTEGER NOT NULL, ResourceKind TEXT NOT NULL CHECK(COALESCE((ResourceKind IN ('VENUE','TRADING','GAME_INPUTS','EVENT_QUEUE')),0)),
 ResourceKey TEXT NOT NULL, AsOfUtc TEXT NOT NULL, VersionHash TEXT NOT NULL,
 PRIMARY KEY(WorldID,ContextID,ResourceKind,ResourceKey), FOREIGN KEY(WorldID,ContextID) REFERENCES WorldExecutionContexts(WorldID,ContextID)
);
CREATE TABLE BroadcastSessions (
 BroadcastID INTEGER PRIMARY KEY, WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, ActorUserID INTEGER NOT NULL,
 SessionKey TEXT NOT NULL, CursorElapsedSeconds INTEGER NOT NULL CHECK(COALESCE((CursorElapsedSeconds>=0),0)), LastFactID INTEGER NOT NULL DEFAULT 0,
 Mode TEXT NOT NULL CHECK(COALESCE((Mode IN ('LIVE','REPLAY')),0)), FilterVersion TEXT NOT NULL,
 UNIQUE(WorldID,BroadcastID), UNIQUE(WorldID,GameID,ActorUserID,SessionKey), FOREIGN KEY(WorldID,GameID) REFERENCES GameRuns(WorldID,GameID),
 FOREIGN KEY(WorldID,ActorUserID) REFERENCES WorldUsers(WorldID,WorldUserID)
);
CREATE TABLE BroadcastDeliveries (
 WorldID INTEGER NOT NULL, BroadcastID INTEGER NOT NULL, FactID INTEGER NOT NULL, DeliveredAtUtc TEXT NOT NULL,
 DeliveryOrdinal INTEGER NOT NULL CHECK(COALESCE((DeliveryOrdinal>0),0)), ContentBindingHash TEXT NOT NULL,
 PRIMARY KEY(WorldID,BroadcastID,FactID), UNIQUE(WorldID,BroadcastID,DeliveryOrdinal),
 FOREIGN KEY(WorldID,BroadcastID) REFERENCES BroadcastSessions(WorldID,BroadcastID),
 FOREIGN KEY(WorldID,FactID) REFERENCES WorldEventFacts(WorldID,FactID)
);
CREATE VIEW GameHistoricalProgress AS SELECT c.WorldID,c.GameID,c.Generation,c.PeriodNo,c.RemainingSeconds,c.ElapsedSeconds,c.State,
 strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||c.ElapsedSeconds||' seconds') AS HistoricalUtc
 FROM GameExecutionCheckpoints c JOIN GameRuns r ON r.WorldID=c.WorldID AND r.GameID=c.GameID
 WHERE NOT EXISTS(SELECT 1 FROM GameExecutionCheckpoints n WHERE n.WorldID=c.WorldID AND n.GameID=c.GameID AND n.Generation>c.Generation);
CREATE VIEW WorldVenueProjection AS SELECT v.* FROM TemporalVenueVersions v JOIN Worlds w ON w.WorldID=v.WorldID
 WHERE v.EffectiveUtc<=w.CurrentTimeUtc AND NOT EXISTS(SELECT 1 FROM TemporalVenueVersions n WHERE n.WorldID=v.WorldID
 AND n.WorldVenueID=v.WorldVenueID AND n.EffectiveUtc<=w.CurrentTimeUtc
 AND (n.EffectiveUtc>v.EffectiveUtc OR (n.EffectiveUtc=v.EffectiveUtc AND (n.EffectiveOrder>v.EffectiveOrder OR (n.EffectiveOrder=v.EffectiveOrder AND n.DataRevision>v.DataRevision)))));
CREATE VIEW WorldTradingProjection AS SELECT v.* FROM SeasonTradingVersions v JOIN Worlds w ON w.WorldID=v.WorldID
 WHERE v.EffectiveUtc<=w.CurrentTimeUtc AND NOT EXISTS(SELECT 1 FROM SeasonTradingVersions n WHERE n.WorldID=v.WorldID
 AND n.CompetitionSeasonID=v.CompetitionSeasonID AND n.EffectiveUtc<=w.CurrentTimeUtc
 AND (n.EffectiveUtc>v.EffectiveUtc OR (n.EffectiveUtc=v.EffectiveUtc AND (n.EffectiveOrder>v.EffectiveOrder OR (n.EffectiveOrder=v.EffectiveOrder AND n.DataRevision>v.DataRevision)))));

ALTER TABLE WorldExecutionContexts ADD COLUMN OrderInInstant INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((OrderInInstant>=0),0));
ALTER TABLE TemporalVenueVersions ADD COLUMN EffectiveOrder INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((EffectiveOrder>=0),0));
ALTER TABLE SeasonTradingVersions ADD COLUMN EffectiveOrder INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((EffectiveOrder>=0),0));
ALTER TABLE CausalReadFences ADD COLUMN AsOfOrder INTEGER NOT NULL DEFAULT 0 CHECK(COALESCE((AsOfOrder>=0),0));
CREATE TRIGGER CW_WindowInsert BEFORE INSERT ON GameProcessingWindows
WHEN NEW.State<>'DRAFT' OR NOT EXISTS(SELECT 1 FROM CompetitionSeasons cs JOIN WorldCompetitions c ON c.WorldCompetitionID=cs.WorldCompetitionID
 JOIN WorldUsers u ON u.WorldID=c.WorldID WHERE cs.CompetitionSeasonID=NEW.CompetitionSeasonID AND c.WorldID=NEW.WorldID
 AND cs.Status='ACTIVE' AND c.Active=1 AND u.WorldUserID=NEW.CreatedByUserID AND u.Active=1 AND u.CanAdvanceWorld=1)
BEGIN SELECT RAISE(ABORT,'window needs scoped authorized draft'); END;
CREATE TRIGGER CW_MemberInsert BEFORE INSERT ON GameWindowMembers
WHEN NOT EXISTS(SELECT 1 FROM GameProcessingWindows w JOIN GameKickoffOccurrences k ON k.WorldID=w.WorldID
 JOIN WorldScheduledGames g ON g.WorldScheduledGameID=k.WorldScheduledGameID WHERE w.WorldID=NEW.WorldID AND w.WindowID=NEW.WindowID
 AND w.State='DRAFT' AND k.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND k.WorldScheduledGameID=NEW.WorldScheduledGameID
 AND k.IsCurrent=1 AND g.CompetitionSeasonID=w.CompetitionSeasonID AND g.ScheduledUtc=w.KickoffUtc)
BEGIN SELECT RAISE(ABORT,'freeze exact scoped kickoff cohort before admission'); END;
CREATE TRIGGER CW_WindowUpdate BEFORE UPDATE ON GameProcessingWindows
WHEN NEW.WindowID<>OLD.WindowID OR NEW.WorldID<>OLD.WorldID OR NEW.CompetitionSeasonID<>OLD.CompetitionSeasonID OR NEW.WindowKey<>OLD.WindowKey
 OR NEW.KickoffUtc<>OLD.KickoffUtc OR NEW.MembershipHash<>OLD.MembershipHash OR NEW.CreatedByUserID<>OLD.CreatedByUserID OR NEW.CreatedRevision<>OLD.CreatedRevision
 OR NOT ((OLD.State='DRAFT' AND NEW.State='ADMITTED') OR (OLD.State='ADMITTED' AND NEW.State IN ('PAUSED','RESOLVED')) OR (OLD.State='PAUSED' AND NEW.State IN ('ADMITTED','RESOLVED')))
BEGIN SELECT RAISE(ABORT,'immutable window binding or invalid window lifecycle'); END)CLOCK_SQL";
    sql += R"CLOCK_SQL(;
CREATE TRIGGER CW_Admit BEFORE UPDATE OF State ON GameProcessingWindows WHEN OLD.State='DRAFT' AND NEW.State='ADMITTED' AND (
 NEW.KickoffUtc<>(SELECT CurrentTimeUtc FROM Worlds WHERE WorldID=NEW.WorldID)
 OR (SELECT count(*) FROM GameWindowMembers m WHERE m.WorldID=NEW.WorldID AND m.WindowID=NEW.WindowID)=0
 OR EXISTS(SELECT 1 FROM WorldScheduledGames g WHERE g.CompetitionSeasonID=NEW.CompetitionSeasonID AND g.ScheduledUtc=NEW.KickoffUtc
 AND NOT EXISTS(SELECT 1 FROM GameWindowMembers m WHERE m.WorldID=NEW.WorldID AND m.WindowID=NEW.WindowID AND m.WorldScheduledGameID=g.WorldScheduledGameID))
 OR EXISTS(SELECT 1 FROM GameKickoffOccurrences k JOIN WorldScheduledGames g ON g.WorldScheduledGameID=k.WorldScheduledGameID
 WHERE k.WorldID=NEW.WorldID AND k.IsCurrent=1 AND g.ScheduledUtc<NEW.KickoffUtc AND NOT EXISTS(
 SELECT 1 FROM GameWindowMembers m JOIN GameRuns r ON r.WorldID=m.WorldID AND r.ScheduledWorldEventID=m.ScheduledWorldEventID
 JOIN GameFinalSaves f ON f.WorldID=r.WorldID AND f.GameID=r.GameID WHERE m.WorldID=k.WorldID AND m.WorldScheduledGameID=k.WorldScheduledGameID)))
BEGIN SELECT RAISE(ABORT,'window admission needs whole cohort and earlier resolution receipts'); END;
CREATE TRIGGER CW_ResolveWindow BEFORE UPDATE OF State ON GameProcessingWindows WHEN NEW.State='RESOLVED' AND EXISTS(
 SELECT 1 FROM GameWindowMembers m WHERE m.WorldID=NEW.WorldID AND m.WindowID=NEW.WindowID AND NOT EXISTS(
 SELECT 1 FROM GameRuns r JOIN GameFinalSaves f ON f.WorldID=r.WorldID AND f.GameID=r.GameID
 WHERE r.WorldID=m.WorldID AND r.ScheduledWorldEventID=m.ScheduledWorldEventID AND r.State='FINAL'))
BEGIN SELECT RAISE(ABORT,'every window member needs a durable final save'); END;
CREATE TRIGGER CW_RunInsert BEFORE INSERT ON GameRuns WHEN NEW.State<>'PREPARED' OR NOT EXISTS(
 SELECT 1 FROM GameProcessingWindows w JOIN GameWindowMembers m ON m.WorldID=w.WorldID AND m.WindowID=w.WindowID
 JOIN Games g ON g.WorldID=m.WorldID AND g.WorldScheduledGameID=m.WorldScheduledGameID
 JOIN GameArtifacts a ON a.GameID=g.GameID WHERE w.WorldID=NEW.WorldID AND w.WindowID=NEW.WindowID AND w.State='ADMITTED'
 AND m.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND g.GameID=NEW.GameID AND w.KickoffUtc=NEW.KickoffUtc
 AND g.SimulationProviderKey=NEW.ProviderKey AND g.SimulationProviderVersion=NEW.ProviderVersion AND a.GameArtifactID=NEW.InputArtifactID)
BEGIN SELECT RAISE(ABORT,'game run needs admitted fixture and bound provider/input artifact'); END;
CREATE TRIGGER CW_RunUpdate BEFORE UPDATE ON GameRuns WHEN NEW.WorldID<>OLD.WorldID OR NEW.GameID<>OLD.GameID OR NEW.WindowID<>OLD.WindowID
 OR NEW.ScheduledWorldEventID<>OLD.ScheduledWorldEventID OR NEW.KickoffUtc<>OLD.KickoffUtc OR NEW.ProfileKey<>OLD.ProfileKey OR NEW.ProfileVersion<>OLD.ProfileVersion
 OR NEW.ProviderKey<>OLD.ProviderKey OR NEW.ProviderVersion<>OLD.ProviderVersion OR NEW.InputBindingsHash<>OLD.InputBindingsHash OR NEW.InputArtifactID<>OLD.InputArtifactID
 OR NEW.InitialVenueVersionID IS NOT OLD.InitialVenueVersionID OR NOT ((OLD.State='PREPARED' AND NEW.State='RUNNING') OR (OLD.State='RUNNING' AND NEW.State IN ('PAUSED','FINAL')) OR (OLD.State='PAUSED' AND NEW.State IN ('RUNNING','FINAL')))
BEGIN SELECT RAISE(ABORT,'game inputs immutable; valid lifecycle required'); END;
CREATE TRIGGER CW_RunFinal BEFORE UPDATE ON GameRuns WHEN NEW.State='FINAL' AND NOT EXISTS(
 SELECT 1 FROM GameFinalSaves f WHERE f.WorldID=NEW.WorldID AND f.GameID=NEW.GameID)
BEGIN SELECT RAISE(ABORT,'final run requires final-save receipt'); END;
CREATE TRIGGER CW_PeriodInsert BEFORE INSERT ON GamePeriods WHEN NOT EXISTS(
 SELECT 1 FROM GameRuns r JOIN GameClockProfiles p ON p.WorldID=r.WorldID AND p.ProfileKey=r.ProfileKey AND p.ProfileVersion=r.ProfileVersion
 WHERE r.WorldID=NEW.WorldID AND r.GameID=NEW.GameID AND
 ((NEW.PeriodKind='REGULATION' AND NEW.DurationSeconds=p.QuarterSeconds AND r.State='PREPARED')
 OR (NEW.PeriodKind='OVERTIME' AND p.OvertimePolicyKey<>'UNSUPPORTED' AND r.State IN ('RUNNING','PAUSED'))))
BEGIN SELECT RAISE(ABORT,'period must bind configured regulation or supported overtime policy'); END;
CREATE TRIGGER CW_ClaimInsert BEFORE INSERT ON GameExecutionClaims WHEN NEW.State<>'OPEN' OR NOT EXISTS(
 SELECT 1 FROM GameRuns r WHERE r.WorldID=NEW.WorldID AND r.GameID=NEW.GameID AND r.State IN ('PREPARED','RUNNING','PAUSED')
 AND r.InputBindingsHash=NEW.InputBindingsHash AND NEW.ExpectedGeneration=COALESCE((SELECT max(c.Generation) FROM GameExecutionCheckpoints c WHERE c.WorldID=r.WorldID AND c.GameID=r.GameID),-1)
 AND NEW.TargetElapsedSeconds>=COALESCE((SELECT max(c.ElapsedSeconds) FROM GameExecutionCheckpoints c WHERE c.WorldID=r.WorldID AND c.GameID=r.GameID),0))
BEGIN SELECT RAISE(ABORT,'claim must bind current generation/input and forward elapsed target'); END;
CREATE TRIGGER CW_ClaimUpdate BEFORE UPDATE ON GameExecutionClaims WHEN NEW.ClaimID<>OLD.ClaimID OR NEW.WorldID<>OLD.WorldID OR NEW.GameID<>OLD.GameID
 OR NEW.CommandKey<>OLD.CommandKey OR NEW.ExpectedGeneration<>OLD.ExpectedGeneration OR NEW.TargetElapsedSeconds<>OLD.TargetElapsedSeconds
 OR NEW.ProviderStepHash<>OLD.ProviderStepHash OR NEW.InputBindingsHash<>OLD.InputBindingsHash OR OLD.State<>'OPEN' OR NEW.State NOT IN ('COMMITTED','CANCELED')
BEGIN SELECT RAISE(ABORT,'immutable claim; finish or cancel once'); END;
CREATE TRIGGER CW_Checkpoint BEFORE INSERT ON GameExecutionCheckpoints WHEN NOT EXISTS(
 SELECT 1 FROM GameExecutionClaims c JOIN GamePeriods p ON p.WorldID=c.WorldID AND p.GameID=c.GameID AND p.PeriodNo=NEW.PeriodNo
 WHERE c.WorldID=NEW.WorldID AND c.GameID=NEW.GameID AND c.ClaimID=NEW.ClaimID AND c.State='OPEN'
 AND NEW.Generation=c.ExpectedGeneration+1 AND NEW.ElapsedSeconds=c.TargetElapsedSeconds
 AND ((p.ClockMode='COUNTDOWN' AND NEW.RemainingSeconds BETWEEN 0 AND p.DurationSeconds) OR (p.ClockMode='UNTIMED' AND NEW.RemainingSeconds IS NULL))
 AND ((NEW.Generation=0 AND NEW.PeriodNo=1 AND NEW.RemainingSeconds=p.DurationSeconds AND NEW.ElapsedSeconds=0) OR EXISTS(SELECT 1 FROM GameExecutionCheckpoints old WHERE old.WorldID=NEW.WorldID AND old.GameID=NEW.GameID AND old.Generation=NEW.Generation-1
 AND old.State<>'FINAL' AND old.ElapsedSeconds<=NEW.ElapsedSeconds AND
 ((old.PeriodNo=NEW.PeriodNo AND (NEW.RemainingSeconds<=old.RemainingSeconds OR (NEW.RemainingSeconds IS NULL AND old.RemainingSeconds IS NULL)))
 OR (NEW.PeriodNo=old.PeriodNo+1 AND old.RemainingSeconds=0 AND ((p.ClockMode='COUNTDOWN' AND NEW.RemainingSeconds=p.DurationSeconds) OR (p.ClockMode='UNTIMED' AND NEW.RemainingSeconds IS NULL)))))))
BEGIN SELECT RAISE(ABORT,'invalid generation, countdown transition or elapsed checkpoint'); END;
CREATE TRIGGER CW_Interval BEFORE INSERT ON GameTimingIntervals WHEN NOT EXISTS(
 SELECT 1 FROM GameExecutionClaims c WHERE c.WorldID=NEW.WorldID AND c.GameID=NEW.GameID AND c.ClaimID=NEW.ClaimID
 AND c.State='OPEN' AND NEW.EndsElapsedSeconds<=c.TargetElapsedSeconds)
 OR EXISTS(SELECT 1 FROM GameTimingIntervals i WHERE i.WorldID=NEW.WorldID AND i.GameID=NEW.GameID
 AND i.StartsElapsedSeconds<NEW.EndsElapsedSeconds AND NEW.StartsElapsedSeconds<i.EndsElapsedSeconds)
BEGIN SELECT RAISE(ABORT,'elapsed interval needs live claim and cannot double count time'); END;
CREATE TRIGGER CW_FinalSave BEFORE INSERT ON GameFinalSaves WHEN NOT EXISTS(
 SELECT 1 FROM GameExecutionCheckpoints c JOIN GameRuns r ON r.WorldID=c.WorldID AND r.GameID=c.GameID
 JOIN Games g ON g.WorldID=r.WorldID AND g.GameID=r.GameID
 WHERE c.WorldID=NEW.WorldID AND c.GameID=NEW.GameID AND c.Generation=NEW.Generation AND c.State='FINAL'
 AND c.ElapsedSeconds=NEW.FinalElapsedSeconds AND ((c.PeriodNo=4 AND c.RemainingSeconds=0) OR c.PeriodNo>4) AND (SELECT count(*) FROM GamePeriods p WHERE p.WorldID=r.WorldID AND p.GameID=r.GameID AND p.PeriodKind='REGULATION')=4 AND NEW.CompletedUtc=strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||c.ElapsedSeconds||' seconds')
 AND g.GameStatus='FINAL' AND g.CompletedUtc=NEW.CompletedUtc)
 OR COALESCE((SELECT sum(i.EndsElapsedSeconds-i.StartsElapsedSeconds) FROM GameTimingIntervals i WHERE i.WorldID=NEW.WorldID AND i.GameID=NEW.GameID),0)<>NEW.FinalElapsedSeconds
BEGIN SELECT RAISE(ABORT,'final save needs final checkpoint/game and gap-free elapsed coverage'); END;
CREATE TRIGGER CW_Context BEFORE INSERT ON WorldExecutionContexts WHEN NOT EXISTS(
 SELECT 1 FROM Worlds w JOIN WorldUsers u ON u.WorldID=w.WorldID JOIN WorldDataRevisions d ON d.WorldID=w.WorldID
 WHERE w.WorldID=NEW.WorldID AND u.WorldUserID=NEW.ActorUserID AND u.Active=1 AND NEW.WorldAnchorUtc=w.CurrentTimeUtc AND NEW.DataRevision=d.Revision+1 AND
 ((NEW.DriverKind='WORLD' AND u.CanAdvanceWorld=1 AND NEW.EffectiveUtc=w.CurrentTimeUtc AND EXISTS(
 SELECT 1 FROM WorldAdvanceRequests a WHERE a.WorldID=w.WorldID AND a.AdvanceRequestID=NEW.AdvanceRequestID AND a.State='RUNNING'))
 OR (NEW.DriverKind='ADMIN' AND u.CanEnableGodMode=1 AND NEW.EffectiveUtc=w.CurrentTimeUtc AND EXISTS(
 SELECT 1 FROM WorldUserEditModes m WHERE m.WorldID=u.WorldID AND m.WorldUserID=u.WorldUserID AND m.GodModeEnabled=1))
 OR (NEW.DriverKind='EDIT' AND NEW.EffectiveUtc=w.CurrentTimeUtc)
 OR (NEW.DriverKind='GAME' AND u.CanAdvanceWorld=1 AND EXISTS(SELECT 1 FROM GameExecutionClaims cl JOIN GameRuns r ON r.WorldID=cl.WorldID AND r.GameID=cl.GameID
 WHERE cl.WorldID=w.WorldID AND cl.ClaimID=NEW.ClaimID AND cl.GameID=NEW.GameID AND cl.State='OPEN'
 AND NEW.EffectiveUtc>=COALESCE((SELECT h.HistoricalUtc FROM GameHistoricalProgress h WHERE h.WorldID=r.WorldID AND h.GameID=r.GameID),r.KickoffUtc) AND NEW.EffectiveUtc<=strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||cl.TargetElapsedSeconds||' seconds')))))
BEGIN SELECT RAISE(ABORT,'context must bind trusted current World or claimed game historical range'); END;
CREATE TRIGGER CW_EventPriority BEFORE UPDATE ON ScheduledWorldEvents WHEN NEW.DispatchPriority<>OLD.DispatchPriority
BEGIN SELECT RAISE(ABORT,'published priority immutable'); END;
CREATE TRIGGER CW_Outcome BEFORE INSERT ON EventOutcomes WHEN NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c JOIN ScheduledWorldEvents e ON e.WorldID=c.WorldID
 WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ContextID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 AND e.State='PENDING' AND e.KindKey<>'GAME_KICKOFF' AND e.StartsUtc=NEW.HistoricalUtc AND c.EffectiveUtc=NEW.HistoricalUtc)
 OR EXISTS(SELECT 1 FROM EventDependencies d JOIN ScheduledWorldEvents e ON e.WorldID=d.WorldID AND e.ScheduledWorldEventID=d.RequiresEventID
 WHERE d.WorldID=NEW.WorldID AND d.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND e.State<>'COMPLETED')
BEGIN SELECT RAISE(ABORT,'resolve typed due event once in historical context after prerequisites'); END;
CREATE TRIGGER CW_VenueVersion BEFORE INSERT ON TemporalVenueVersions WHEN NEW.ContextID IS NOT NULL AND NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ContextID AND c.EffectiveUtc=NEW.EffectiveUtc AND c.OrderInInstant=NEW.EffectiveOrder)
BEGIN SELECT RAISE(ABORT,'temporal venue version needs exact effective context'); END;
CREATE TRIGGER CW_VenueFence BEFORE INSERT ON TemporalVenueVersions WHEN EXISTS(
 SELECT 1 FROM CausalReadFences f WHERE f.WorldID=NEW.WorldID AND f.ResourceKind='VENUE' AND f.ResourceKey=CAST(NEW.WorldVenueID AS TEXT)
 AND f.ContextID IS NOT NEW.ContextID AND (NEW.EffectiveUtc<f.AsOfUtc OR (NEW.EffectiveUtc=f.AsOfUtc AND NEW.EffectiveOrder<=f.AsOfOrder)))
BEGIN SELECT RAISE(ABORT,'CAUSAL_FENCE: preserve sealed venue dependency'); END;
CREATE TRIGGER CW_TradingFence BEFORE INSERT ON SeasonTradingVersions WHEN EXISTS(
 SELECT 1 FROM CausalReadFences f WHERE f.WorldID=NEW.WorldID AND f.ResourceKind='TRADING' AND f.ResourceKey=CAST(NEW.CompetitionSeasonID AS TEXT)
 AND (NEW.EffectiveUtc<f.AsOfUtc OR (NEW.EffectiveUtc=f.AsOfUtc AND NEW.EffectiveOrder<=f.AsOfOrder))
 AND NOT EXISTS(SELECT 1 FROM EventOutcomes o WHERE o.WorldID=f.WorldID AND o.OutcomeID=NEW.OutcomeID AND o.ContextID=f.ContextID))
BEGIN SELECT RAISE(ABORT,'CAUSAL_FENCE: preserve sealed tra)CLOCK_SQL";
    sql += R"CLOCK_SQL(ding dependency'); END;
CREATE TRIGGER CW_Fact BEFORE INSERT ON WorldEventFacts WHEN NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ExecutionContextID
 AND c.EffectiveUtc=NEW.OccurredUtc AND c.DataRevision=NEW.DataRevision)
BEGIN SELECT RAISE(ABORT,'fact requires typed historical context, not parked-clock forgery'); END;
CREATE TRIGGER CW_VenueResolve BEFORE UPDATE ON VenueChangeEvents WHEN NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c JOIN VenueChangePlans p ON p.WorldID=c.WorldID AND p.PlanID=NEW.PlanID
 JOIN ScheduledWorldEvents e ON e.WorldID=c.WorldID AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID
 WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ResolutionContextID AND c.EffectiveUtc=NEW.ResolvedUtc AND c.DataRevision=NEW.ResolvedRevision AND
 ((NEW.State='APPLIED' AND NEW.ResolutionReason='DUE_TIME' AND c.EffectiveUtc=p.DueUtc AND e.State='PENDING' AND EXISTS(
 SELECT 1 FROM TemporalVenueVersions v WHERE v.WorldID=c.WorldID AND v.ContextID=c.ContextID AND v.SourceEventID=e.ScheduledWorldEventID))
 OR (c.DriverKind IN ('WORLD','ADMIN') AND NEW.State='CANCELED' AND NEW.ResolutionReason='GOD_OVERRIDE' AND e.State='CANCELED')
 OR (c.EffectiveUtc<p.DueUtc AND ((NEW.State='CANCELED' AND NEW.ResolutionReason='USER_CANCEL' AND e.State='CANCELED')
 OR (NEW.State='SUPERSEDED' AND e.State='SUPERSEDED')))))
BEGIN SELECT RAISE(ABORT,'venue resolution needs temporal effect or permitted cancellation context'); END;
CREATE TRIGGER CW_StartGame BEFORE UPDATE OF State ON ScheduledWorldEvents WHEN NEW.State='RUNNING' AND NOT EXISTS(
 SELECT 1 FROM GameRuns r JOIN GameProcessingWindows w ON w.WorldID=r.WorldID AND w.WindowID=r.WindowID
 JOIN GameVenueEvidence v ON v.WorldID=r.WorldID AND v.GameID=r.GameID
 WHERE r.WorldID=NEW.WorldID AND r.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND r.State='RUNNING'
 AND w.State='ADMITTED' AND w.KickoffUtc=NEW.StartsUtc AND v.CapturedUtc=NEW.StartsUtc)
BEGIN SELECT RAISE(ABORT,'kickoff needs admitted window and frozen historical evidence'); END;
CREATE TRIGGER CW_StartDue BEFORE UPDATE OF State ON ScheduledWorldEvents WHEN NEW.State='RUNNING' AND EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=NEW.WorldID AND e.State='PENDING' AND e.KindKey<>'GAME_KICKOFF' AND e.StartsUtc<=NEW.StartsUtc) BEGIN SELECT RAISE(ABORT,'settle automatic work before kickoff'); END;
CREATE TRIGGER CW_CompleteEvent BEFORE UPDATE OF State ON ScheduledWorldEvents WHEN NEW.State='COMPLETED' AND NOT (
 (NEW.KindKey='GAME_KICKOFF' AND EXISTS(SELECT 1 FROM GameRuns r JOIN GameFinalSaves f ON f.WorldID=r.WorldID AND f.GameID=r.GameID
 WHERE r.WorldID=NEW.WorldID AND r.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND f.CompletedUtc=NEW.ResolvedUtc))
 OR (NEW.KindKey<>'GAME_KICKOFF' AND EXISTS(SELECT 1 FROM EventOutcomes o WHERE o.WorldID=NEW.WorldID AND o.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND o.HistoricalUtc=NEW.ResolvedUtc)))
BEGIN SELECT RAISE(ABORT,'complete once from genuine final save or shared typed outcome'); END;
CREATE TRIGGER CW_Receipt BEFORE INSERT ON SchedulerDispatchReceipts WHEN NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c JOIN ScheduledWorldEvents e ON e.WorldID=c.WorldID WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ContextID
 AND c.EffectiveUtc=NEW.OccurredUtc AND c.DataRevision=NEW.DataRevision AND e.ScheduledWorldEventID=NEW.ScheduledWorldEventID AND
 ((NEW.StepKey='START' AND e.State='RUNNING' AND e.KindKey='GAME_KICKOFF')
 OR (NEW.StepKey IN ('APPLY','RESOLVE') AND e.State='COMPLETED' AND e.KindKey<>'GAME_KICKOFF')
 OR (NEW.StepKey='COMPLETE' AND e.State='COMPLETED' AND e.KindKey='GAME_KICKOFF')))
BEGIN SELECT RAISE(ABORT,'receipt must match one historical dispatch effect'); END;
CREATE TRIGGER CW_Clock BEFORE UPDATE OF CurrentTimeUtc ON Worlds WHEN NEW.CurrentTimeUtc<OLD.CurrentTimeUtc
 OR EXISTS(SELECT 1 FROM GameProcessingWindows w WHERE w.WorldID=NEW.WorldID AND w.State IN ('ADMITTED','PAUSED') AND NEW.CurrentTimeUtc<>OLD.CurrentTimeUtc)
 OR EXISTS(SELECT 1 FROM ScheduledWorldEvents e WHERE e.WorldID=NEW.WorldID AND e.State='PENDING' AND e.StartsUtc<NEW.CurrentTimeUtc)
BEGIN SELECT RAISE(ABORT,'park World during window; never rewind or skip pending dated work'); END;
CREATE TRIGGER CW_BroadcastUpdate BEFORE UPDATE ON BroadcastSessions WHEN NEW.WorldID<>OLD.WorldID OR NEW.BroadcastID<>OLD.BroadcastID
 OR NEW.GameID<>OLD.GameID OR NEW.ActorUserID<>OLD.ActorUserID OR NEW.SessionKey<>OLD.SessionKey OR NEW.Mode<>OLD.Mode OR NEW.FilterVersion<>OLD.FilterVersion
 OR NEW.CursorElapsedSeconds<OLD.CursorElapsedSeconds OR NEW.LastFactID<OLD.LastFactID
BEGIN SELECT RAISE(ABORT,'broadcast cursor forward-only; new replay needs a new session'); END;
CREATE TRIGGER CW_Delivery BEFORE INSERT ON BroadcastDeliveries WHEN NOT EXISTS(
 SELECT 1 FROM BroadcastSessions b JOIN GameRuns r ON r.WorldID=b.WorldID AND r.GameID=b.GameID
 JOIN WorldEventFacts f ON f.WorldID=b.WorldID WHERE b.WorldID=NEW.WorldID AND b.BroadcastID=NEW.BroadcastID AND f.FactID=NEW.FactID
 AND NEW.DeliveredAtUtc=strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||b.CursorElapsedSeconds||' seconds') AND f.OccurredUtc<=NEW.DeliveredAtUtc
 AND (f.Visibility='PUBLIC' OR (f.Visibility='RESTRICTED' AND EXISTS(SELECT 1 FROM WorldFactReaders k WHERE k.WorldID=f.WorldID AND k.FactID=f.FactID
 AND k.WorldUserID=b.ActorUserID AND k.KnownFromUtc<=NEW.DeliveredAtUtc))))
BEGIN SELECT RAISE(ABORT,'no future/private broadcast delivery outside local historical cursor'); END;

CREATE TRIGGER CW_GameClockProfiles_Immutable BEFORE UPDATE ON GameClockProfiles BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameClockProfiles_Retain BEFORE DELETE ON GameClockProfiles BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameWindowMembers_Immutable BEFORE UPDATE ON GameWindowMembers BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameWindowMembers_Retain BEFORE DELETE ON GameWindowMembers BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GamePeriods_Immutable BEFORE UPDATE ON GamePeriods BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GamePeriods_Retain BEFORE DELETE ON GamePeriods BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameExecutionCheckpoints_Immutable BEFORE UPDATE ON GameExecutionCheckpoints BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameExecutionCheckpoints_Retain BEFORE DELETE ON GameExecutionCheckpoints BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameTimingIntervals_Immutable BEFORE UPDATE ON GameTimingIntervals BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameTimingIntervals_Retain BEFORE DELETE ON GameTimingIntervals BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameTimingReasons_Immutable BEFORE UPDATE ON GameTimingReasons BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameTimingReasons_Retain BEFORE DELETE ON GameTimingReasons BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GamePlayTiming_Immutable BEFORE UPDATE ON GamePlayTiming BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GamePlayTiming_Retain BEFORE DELETE ON GamePlayTiming BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameFinalSaves_Immutable BEFORE UPDATE ON GameFinalSaves BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_GameFinalSaves_Retain BEFORE DELETE ON GameFinalSaves BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_WorldExecutionContexts_Immutable BEFORE UPDATE ON WorldExecutionContexts BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_WorldExecutionContexts_Retain BEFORE DELETE ON WorldExecutionContexts BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_TemporalVenueVersions_Immutable BEFORE UPDATE ON TemporalVenueVersions BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_TemporalVenueVersions_Retain BEFORE DELETE ON TemporalVenueVersions BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_SeasonEventDefinitions_Immutable BEFORE UPDATE ON SeasonEventDefinitions BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_SeasonEventDefinitions_Retain BEFORE DELETE ON SeasonEventDefinitions BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_TeamHomeTimeZones_Immutable BEFORE UPDATE ON TeamHomeTimeZones BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_TeamHomeTimeZones_Retain BEFORE DELETE ON TeamHomeTimeZones BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_SeasonEventInstances_Immutable BEFORE UPDATE ON SeasonEventInstances BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_SeasonEventInstances_Retain BEFORE DELETE ON SeasonEventInstances BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_EventDependencies_Immutable BEFORE UPDATE ON EventDependencies BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_EventDependencies_Retain BEFORE DELETE ON EventDependencies BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_EventOutcomes_Immutable BEFORE UPDATE ON EventOutcomes BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_EventOutcomes_Retain BEFORE DELETE ON EventOutcomes BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_SeasonTradingVersions_Immutable BEFORE UPDATE ON SeasonTradingVersions BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_SeasonTradingVersions_Retain BEFORE DELETE ON SeasonTradingVersions BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_VenueCeremonyOutcomes_Immutable BEFORE UPDATE ON VenueCeremonyOutcomes BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_VenueCeremonyOutcomes_Retain BEFORE DELETE ON VenueCeremonyOutcomes BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_TemporalEffectReceipts_Immutable BEFORE UPDATE ON TemporalEffectReceipts BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_TemporalEffectReceipts_Retain BEFORE DELETE ON TemporalEffectReceipts BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_CausalReadFences_Immutable BEFORE UPDATE ON CausalReadFences BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_CausalReadFences_Retain BEFORE DELETE ON CausalReadFences BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_BroadcastDeliveries_Immutable BEFORE UPDATE ON BroadcastDeliveries BEGIN SELECT RAISE(ABORT,'immutable clock/event evidence'); END;
CREATE TRIGGER CW_BroadcastDeliveries_Retain BEFORE DELETE ON BroadcastDeliveries BEGIN SELECT RAISE(ABORT,'retain clock/event evidence'); END;

CREATE TRIGGER CW_GameProcessingWindows_Retain BEFORE DELETE ON GameProcessingWindows BEGIN SELECT RAISE(ABORT,'retain processing/receipt history'); END;

CREATE TRIGGER CW_GameRuns_Retain BEFORE DELETE ON GameRuns BEGIN SELECT RAISE(ABORT,'retain processing/receipt history'); END;

CREATE TRIGGER CW_GameExecutionClaims_Retain BEFORE DELETE ON GameExecutionClaims BEGIN SELECT RAISE(ABORT,'retain processing/receipt history'); END;

CREATE TRIGGER CW_BroadcastSessions_Retain BEFORE DELETE ON BroadcastSessions BEGIN SELECT RAISE(ABORT,'retain processing/receipt history'); END;

CREATE TRIGGER CW_SchedulerDispatchReceipts_Retain )CLOCK_SQL";
    sql += R"CLOCK_SQL(BEFORE DELETE ON SchedulerDispatchReceipts BEGIN SELECT RAISE(ABORT,'retain processing/receipt history'); END;
CREATE TRIGGER CW_ReceiptImmutable BEFORE UPDATE ON SchedulerDispatchReceipts BEGIN SELECT RAISE(ABORT,'immutable dispatch receipt'); END;

CREATE TRIGGER CW_CheckpointDue BEFORE INSERT ON GameExecutionCheckpoints WHEN EXISTS(
 SELECT 1 FROM ScheduledWorldEvents e JOIN GameRuns r ON r.WorldID=e.WorldID WHERE r.WorldID=NEW.WorldID AND r.GameID=NEW.GameID
 AND e.State='PENDING' AND e.KindKey<>'GAME_KICKOFF' AND
 (e.StartsUtc<strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||NEW.ElapsedSeconds||' seconds')
 OR (e.StartsUtc=strftime('%Y-%m-%dT%H:%M:%SZ',r.KickoffUtc,'+'||NEW.ElapsedSeconds||' seconds') AND NEW.State<>'PAUSED')))
BEGIN SELECT RAISE(ABORT,'game progress must resolve due shared work or pause at its boundary'); END;
CREATE TRIGGER CW_BroadcastInsert BEFORE INSERT ON BroadcastSessions WHEN NOT EXISTS(
 SELECT 1 FROM GameHistoricalProgress h JOIN WorldUsers u ON u.WorldID=h.WorldID WHERE h.WorldID=NEW.WorldID AND h.GameID=NEW.GameID
 AND NEW.CursorElapsedSeconds<=h.ElapsedSeconds AND u.WorldUserID=NEW.ActorUserID AND u.Active=1)
BEGIN SELECT RAISE(ABORT,'broadcast requires authorized service context within saved game progress'); END;
CREATE TRIGGER CW_BroadcastBound BEFORE UPDATE ON BroadcastSessions WHEN NEW.CursorElapsedSeconds>
 COALESCE((SELECT max(c.ElapsedSeconds) FROM GameExecutionCheckpoints c WHERE c.WorldID=NEW.WorldID AND c.GameID=NEW.GameID),-1)
BEGIN SELECT RAISE(ABORT,'broadcast cannot exceed actual saved local game progress'); END;
CREATE TRIGGER CW_EffectBinding BEFORE INSERT ON TemporalEffectReceipts WHEN
 (NEW.VenueVersionID IS NOT NULL AND NOT EXISTS(SELECT 1 FROM TemporalVenueVersions v JOIN EventOutcomes o ON o.WorldID=v.WorldID
 WHERE v.WorldID=NEW.WorldID AND v.VersionID=NEW.VenueVersionID AND o.OutcomeID=NEW.OutcomeID AND o.ContextID=v.ContextID AND v.SourceEventID=o.ScheduledWorldEventID))
 OR (NEW.TradingVersionID IS NOT NULL AND NOT EXISTS(SELECT 1 FROM SeasonTradingVersions v WHERE v.WorldID=NEW.WorldID
 AND v.VersionID=NEW.TradingVersionID AND v.OutcomeID=NEW.OutcomeID))
BEGIN SELECT RAISE(ABORT,'effect receipt must bind its typed owning outcome'); END;
CREATE TRIGGER CW_TradingContext BEFORE INSERT ON SeasonTradingVersions WHEN NEW.OutcomeID IS NOT NULL AND NOT EXISTS(
 SELECT 1 FROM EventOutcomes o JOIN WorldExecutionContexts c ON c.WorldID=o.WorldID AND c.ContextID=o.ContextID
 WHERE o.WorldID=NEW.WorldID AND o.OutcomeID=NEW.OutcomeID AND o.HistoricalUtc=NEW.EffectiveUtc AND c.OrderInInstant=NEW.EffectiveOrder AND c.DataRevision=NEW.DataRevision)
BEGIN SELECT RAISE(ABORT,'trading effect needs exact outcome historical context'); END;
CREATE TRIGGER CW_ResultContext BEFORE INSERT ON FinalizedLeagueGameResults WHEN NOT EXISTS(
 SELECT 1 FROM GameFinalSaves f JOIN WorldExecutionContexts c ON c.WorldID=f.WorldID
 WHERE f.WorldID=NEW.WorldID AND f.GameID=NEW.GameID AND c.ContextID=NEW.ExecutionContextID
 AND f.CompletedUtc=NEW.CompletedUtc AND c.EffectiveUtc=NEW.CompletedUtc AND c.DataRevision=NEW.FinalizedRevision)
BEGIN SELECT RAISE(ABORT,'effective result receipt must bind genuine parked-clock final save and historical context'); END;
CREATE TRIGGER CW_VenueEvidence BEFORE INSERT ON GameVenueEvidence WHEN NOT EXISTS(
 SELECT 1 FROM GameRuns r JOIN TemporalVenueVersions v ON v.WorldID=r.WorldID AND v.VersionID=r.InitialVenueVersionID
 WHERE r.WorldID=NEW.WorldID AND r.GameID=NEW.GameID AND NEW.CapturedUtc=r.KickoffUtc AND v.EffectiveUtc<=r.KickoffUtc
 AND v.WorldVenueID=NEW.WorldVenueID AND v.VenueName=NEW.VenueName AND v.TurfType IS NEW.TurfType)
 AND NEW.EvidenceKind<>'UNKNOWN_VENUE'
BEGIN SELECT RAISE(ABORT,'venue evidence must match frozen as-of kickoff version'); END;

-- Domain permissions for EDIT remain a mandatory editing-service check. GAME v1
-- contexts require the same explicit World advancement authority as window admission.
CREATE TRIGGER CW_IntervalClock BEFORE INSERT ON GameTimingIntervals WHEN NOT EXISTS(
 SELECT 1 FROM GamePeriods p WHERE p.WorldID=NEW.WorldID AND p.GameID=NEW.GameID AND p.PeriodNo=NEW.PeriodNo AND
 ((p.ClockMode='UNTIMED' AND NEW.ClockBeforeSeconds IS NULL AND NEW.ClockAfterSeconds IS NULL)
 OR (p.ClockMode='COUNTDOWN' AND NEW.ClockBeforeSeconds BETWEEN 0 AND p.DurationSeconds
 AND NEW.ClockAfterSeconds BETWEEN 0 AND NEW.ClockBeforeSeconds
 AND NEW.ClockBeforeSeconds-NEW.ClockAfterSeconds<=NEW.EndsElapsedSeconds-NEW.StartsElapsedSeconds)))
BEGIN SELECT RAISE(ABORT,'elapsed timing cannot consume more countdown than elapsed duration'); END;
CREATE TRIGGER CW_VenueAdmin BEFORE INSERT ON TemporalVenueVersions WHEN NEW.ContextID IS NOT NULL AND NEW.SourceEventID IS NULL AND NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ContextID AND c.DriverKind='ADMIN')
BEGIN SELECT RAISE(ABORT,'direct venue change requires God-mode context'); END;
CREATE TRIGGER CW_RunVenue BEFORE INSERT ON GameRuns WHEN NEW.InitialVenueVersionID IS NOT NULL AND NOT EXISTS(
 SELECT 1 FROM TemporalVenueVersions v WHERE v.WorldID=NEW.WorldID AND v.VersionID=NEW.InitialVenueVersionID AND v.EffectiveUtc<=NEW.KickoffUtc)
BEGIN SELECT RAISE(ABORT,'frozen venue version must belong to this World and kickoff history'); END;

CREATE UNIQUE INDEX CW_ContextOrder ON WorldExecutionContexts(WorldID,EffectiveUtc,OrderInInstant);
ALTER TABLE WorldParticipationCommands ADD COLUMN ExecutionContextID INTEGER;
CREATE TRIGGER CW_ParticipationContext BEFORE INSERT ON WorldParticipationCommands WHEN NOT EXISTS(
 SELECT 1 FROM WorldExecutionContexts c WHERE c.WorldID=NEW.WorldID AND c.ContextID=NEW.ExecutionContextID
 AND c.ActorUserID=NEW.WorldUserID AND c.EffectiveUtc=NEW.OccurredUtc AND c.DataRevision=NEW.DataRevision)
BEGIN SELECT RAISE(ABORT,'participation decision needs trusted historical execution context'); END;

ALTER TABLE ScheduledWorldEvents ADD COLUMN CreationContextID INTEGER;
CREATE TRIGGER CW_QueueFence BEFORE UPDATE OF State ON ScheduledWorldEvents WHEN NEW.State='PENDING' AND EXISTS(
 SELECT 1 FROM CausalReadFences f WHERE f.WorldID=NEW.WorldID AND f.ResourceKind='EVENT_QUEUE' AND f.ResourceKey='WORLD'
 AND f.ContextID IS NOT NEW.CreationContextID AND NEW.StartsUtc<=f.AsOfUtc)
BEGIN SELECT RAISE(ABORT,'new backdated queue work conflicts with resolved historical dependencies'); END;
CREATE TRIGGER CW_FinalGameImmutable BEFORE UPDATE ON Games WHEN EXISTS(
 SELECT 1 FROM GameFinalSaves f WHERE f.WorldID=OLD.WorldID AND f.GameID=OLD.GameID)
BEGIN SELECT RAISE(ABORT,'canonical finalized game is immutable'); END;
CREATE TRIGGER CW_FinalGameRetain BEFORE DELETE ON Games WHEN EXISTS(
 SELECT 1 FROM GameFinalSaves f WHERE f.WorldID=OLD.WorldID AND f.GameID=OLD.GameID)
BEGIN SELECT RAISE(ABORT,'retain canonical finalized game'); END;
CREATE TRIGGER CW_FrozenFixture BEFORE UPDATE ON WorldScheduledGames WHEN EXISTS(
 SELECT 1 FROM GameWindowMembers m WHERE m.WorldScheduledGameID=OLD.WorldScheduledGameID)
BEGIN SELECT RAISE(ABORT,'admitted fixture identity and kickoff history are frozen'); END;

CREATE VIEW WorldFinalGameProjection AS
 SELECT g.* FROM Games g JOIN GameFinalSaves f ON f.WorldID=g.WorldID AND f.GameID=g.GameID
 JOIN Worlds w ON w.WorldID=g.WorldID WHERE f.CompletedUtc<=w.CurrentTimeUtc;
CREATE VIEW WorldLeagueWeekProjection AS
 SELECT l.WorldID,l.LeagueWeekID,l.CompetitionSeasonID,l.WeekKey,l.PhaseCode,l.SequenceNo,l.DisplayWeekNumber,
 CASE WHEN l.OpenedUtc IS NULL OR l.OpenedUtc>w.CurrentTimeUtc THEN 'FUTURE'
 WHEN l.ClosedUtc IS NOT NULL AND l.ClosedUtc<=w.CurrentTimeUtc THEN 'CLOSED' ELSE 'OPEN' END AS State,
 CASE WHEN l.OpenedUtc<=w.CurrentTimeUtc THEN l.OpenedUtc END AS OpenedUtc,
 CASE WHEN l.ClosedUtc<=w.CurrentTimeUtc THEN l.ClosedUtc END AS ClosedUtc
 FROM LeagueWeeks l JOIN Worlds w ON w.WorldID=l.WorldID;
CREATE VIEW BroadcastEligibleFacts AS
 SELECT b.WorldID,b.BroadcastID,f.FactID,f.OccurredUtc,f.FactKind,f.FactKey FROM BroadcastSessions b
 JOIN GameRuns g ON g.WorldID=b.WorldID AND g.GameID=b.GameID
 JOIN WorldEventFacts f ON f.WorldID=b.WorldID
 WHERE f.OccurredUtc<=strftime('%Y-%m-%dT%H:%M:%SZ',g.KickoffUtc,'+'||b.CursorElapsedSeconds||' seconds')
 AND (f.Visibility='PUBLIC' OR (f.Visibility='RESTRICTED' AND EXISTS(
 SELECT 1 FROM WorldFactReaders r WHERE r.WorldID=f.WorldID AND r.FactID=f.FactID AND r.WorldUserID=b.ActorUserID
 AND r.KnownFromUtc<=strftime('%Y-%m-%dT%H:%M:%SZ',g.KickoffUtc,'+'||b.CursorElapsedSeconds||' seconds'))))
 AND NOT EXISTS(SELECT 1 FROM BroadcastDeliveries d WHERE d.WorldID=b.WorldID AND d.BroadcastID=b.BroadcastID AND d.FactID=f.FactID);

-- A trusted provider's append-only promise, bound to saved progress. A missing
-- promise is unknown, never permission to precompute past another game.
CREATE TABLE GameSharedEventHorizons (
 WorldID INTEGER NOT NULL, GameID INTEGER NOT NULL, Generation INTEGER NOT NULL,
 ProducesNoSharedEvents INTEGER NOT NULL CHECK(ProducesNoSharedEvents IN (0,1)),
 NoNewWorkBeforeUtc TEXT, ProducerPolicyKey TEXT NOT NULL, ProducerPolicyVersion INTEGER NOT NULL CHECK(ProducerPolicyVersion>0),
 ProofHash TEXT NOT NULL, PRIMARY KEY(WorldID,GameID,Generation),
 FOREIGN KEY(WorldID,GameID,Generation) REFERENCES GameExecutionCheckpoints(WorldID,GameID,Generation),
 CHECK(COALESCE((ProducesNoSharedEvents=1 AND NoNewWorkBeforeUtc IS NULL)
 OR (ProducesNoSharedEvents=0 AND NoNewWorkBeforeUtc IS NOT NULL AND length(NoNewWorkBeforeUtc)=20
 AND strftime('%Y-%m-%dT%H:%M:%SZ',NoNewWorkBeforeUtc)=NoNewWorkBeforeUtc),0))
);
CREATE TRIGGER CW_ProducerImmutable BEFORE UPDATE ON GameSharedEventHorizons BEGIN SELECT RAISE(ABORT,'retain producer promise'); END;
CREATE TRIGGER CW_ProducerRetain BEFORE DELETE ON GameSharedEventHorizons BEGIN SELECT RAISE(ABORT,'retain producer promise'); END;
)CLOCK_SQL";
    // Retain the pre-existing publication lookup indexes.
    sql += R"CLOCK_SQL(
CREATE INDEX WS_VenueFacts ON WorldEventFacts(WorldID,VenueChangeEventID,FactKind);
CREATE INDEX WS_VenuePlans ON VenueChangeEvents(WorldID,PlanID);
CREATE INDEX WS_VenueCommands ON VenueChangeEvents(WorldID,CommandID);
CREATE INDEX WS_VenueResolutions ON VenueChangeEvents(WorldID,ResolutionCommandID);
)CLOCK_SQL";
    return sql;
}
