// Private construction extension. Only fresh Worlds are changed.
void instantiateRosterDepth(CreationContext& c)
{
    namespace S = TeamDataSql;
    const auto version = readCoreMetadata(c.core, "RosterDataContractVersion");
    if (version.empty()) return; // Older Core remains usable; roster reports unavailable.
    if (version != "1") throw WorldDatabaseException("Unsupported roster data contract.");
    S::exec(c.world, R"SQL(
CREATE TABLE WorldRosterDepth (
    RosterAssignmentID INTEGER PRIMARY KEY REFERENCES RosterAssignments(RosterAssignmentID),
    TeamSeasonID INTEGER NOT NULL REFERENCES TeamSeasons(TeamSeasonID),
    WorldPlayerSeasonID INTEGER NOT NULL REFERENCES WorldPlayerSeasons(WorldPlayerSeasonID),
    PositionCode TEXT NOT NULL,
    DepthSlot INTEGER NOT NULL CHECK(DepthSlot > 0),
    UNIQUE(TeamSeasonID,PositionCode,DepthSlot),
    UNIQUE(TeamSeasonID,WorldPlayerSeasonID,PositionCode)
);
CREATE TRIGGER RosterDepthMember BEFORE INSERT ON WorldRosterDepth
WHEN NOT EXISTS(SELECT 1 FROM RosterAssignments r JOIN WorldPlayerSeasons p
 ON p.WorldPlayerSeasonID=r.WorldPlayerSeasonID WHERE r.RosterAssignmentID=NEW.RosterAssignmentID
 AND r.TeamSeasonID=NEW.TeamSeasonID AND r.WorldPlayerSeasonID=NEW.WorldPlayerSeasonID
 AND p.PositionCode=NEW.PositionCode AND r.EndDateUtc IS NULL)
BEGIN SELECT RAISE(ABORT,'Depth assignment must match current roster membership and position'); END;
CREATE TRIGGER RosterDepthMemberUpdate BEFORE UPDATE ON WorldRosterDepth
WHEN NOT EXISTS(SELECT 1 FROM RosterAssignments r JOIN WorldPlayerSeasons p
 ON p.WorldPlayerSeasonID=r.WorldPlayerSeasonID WHERE r.RosterAssignmentID=NEW.RosterAssignmentID
 AND r.TeamSeasonID=NEW.TeamSeasonID AND r.WorldPlayerSeasonID=NEW.WorldPlayerSeasonID
 AND p.PositionCode=NEW.PositionCode AND r.EndDateUtc IS NULL)
BEGIN SELECT RAISE(ABORT,'Depth assignment must match current roster membership and position'); END;
)SQL");
    struct Initial { std::int64_t roster, team, player; std::string position; int depth; };
    std::vector<Initial> initial;
    std::map<std::pair<std::int64_t,std::string>,std::set<int>> reserved;
    S::Query source(c.core, "SELECT p.SourceLocalKey,p.TeamSeasonID,p.Position,d.InitialDepth "
        "FROM Players p JOIN CoreRosterData d ON d.PlayerID=p.PlayerID ORDER BY d.SourceOrder");
    while (source.row())
    {
        const auto team = c.teamSeasonByCoreId.find(source.integer(1));
        if (team == c.teamSeasonByCoreId.end()) continue;
        S::Query member(c.world,"SELECT r.RosterAssignmentID,p.WorldPlayerSeasonID "
            "FROM RosterAssignments r JOIN WorldPlayerSeasons p ON p.WorldPlayerSeasonID=r.WorldPlayerSeasonID "
            "WHERE r.TeamSeasonID=? AND p.CompetitionSeasonID=? AND p.CorePlayerKey=? AND r.EndDateUtc IS NULL",
            {team->second,c.competitionSeasonId,source.text()});
        if (!member.row()) throw WorldDatabaseException("Roster initialization cannot resolve player.");
        Initial row{member.integer(),team->second,member.integer(1),source.text(2),
            source.null(3)?0:static_cast<int>(source.integer(3))};
        if (row.depth && !reserved[{row.team,row.position}].insert(row.depth).second)
            throw WorldDatabaseException("Duplicate explicit depth slot for team/position.");
        initial.push_back(std::move(row));
    }
    for (auto& row : initial)
    {
        auto& used = reserved[{row.team,row.position}];
        if (!row.depth) { row.depth=1; while(used.count(row.depth)) ++row.depth; used.insert(row.depth); }
        S::run(c.world,"INSERT INTO WorldRosterDepth VALUES(?,?,?,?,?)",
            {row.roster,row.team,row.player,row.position,static_cast<std::int64_t>(row.depth)});
    }
    if (initial.size()!=c.result->rosterAssignmentsCreated)
        throw WorldDatabaseException("Incomplete roster source order/depth coverage.");
    S::run(c.world,"INSERT INTO WorldFeatureVersions VALUES(?,'ROSTER_DATA',1)",{c.worldId});
}
