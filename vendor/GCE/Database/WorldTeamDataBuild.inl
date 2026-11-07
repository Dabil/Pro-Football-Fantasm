// Private World construction helpers. Schema/identity rules: TD-DESIGN-001..006.
void instantiateCorePeople(CreationContext& c)
{
    namespace S = TeamDataSql;
    S::Query people(c.core, "SELECT CorePersonKey,FirstName,LastName,DisplayName,BirthDate FROM CorePeople "
                            "ORDER BY CorePersonKey");
    while (people.row())
    {
        S::run(c.world,
               "INSERT INTO People(WorldID,PersonKey,CorePersonKey,FirstName,LastName,DisplayName,BirthDate) "
               "VALUES(?,?,?,?,?,?,?)",
               {c.worldId, people.text(0), people.text(0), S::nullable(people.optionalText(1)),
                S::nullable(people.optionalText(2)), people.text(3), S::nullable(people.optionalText(4))});
        S::run(c.world, "INSERT INTO WorldPersonSourceAliases VALUES(?,?,?,?)",
               {c.worldId, c.worldCoreSnapshotId, people.text(0), S::inserted(c.world)});
        ++c.result->peopleCreated;
    }
    S::run(c.world,"INSERT INTO WorldFeatureVersions VALUES(?,'PERSON_NICKNAMES',1)",{c.worldId});
    for(const auto* key:{"PersonNicknamesNormalizationPolicy","PersonNicknamesUnicodeVersion"})
        S::run(c.world,"INSERT INTO WorldSettings VALUES(?,?,?)",{c.worldId,std::string(key),readCoreMetadata(c.core,key)});
    S::Query aliases(c.world,"SELECT CorePersonKey,PersonID FROM WorldPersonSourceAliases WHERE WorldID=? AND WorldCoreSnapshotID=?",{c.worldId,c.worldCoreSnapshotId});
    while(aliases.row())
    {
        S::Query names(c.core,"SELECT NormalizedNickname,Nickname,DisplayOrder FROM CorePersonNicknames WHERE CorePersonKey=? ORDER BY DisplayOrder,NormalizedNickname COLLATE BINARY",{aliases.text()});
        while(names.row())
        {
            S::run(c.world,"INSERT INTO WorldPersonNicknames VALUES(?,?,?,?,?,'CORE',0)",{c.worldId,aliases.integer(1),names.text(),names.text(1),names.integer(2)});
            S::run(c.world,"INSERT INTO WorldPersonNicknameSources VALUES(?,?,?,?,?)",{c.worldId,aliases.integer(1),names.text(),c.worldCoreSnapshotId,aliases.text()});
        }
    }
}
void instantiateTeamData(CreationContext& c, std::int64_t coreSeason, const std::string& utc)
{
    namespace S = TeamDataSql;
    auto* db = c.world;
    S::run(db, "INSERT INTO WorldDataRevisions VALUES(?,0)", {c.worldId});
    S::run(db,
           "INSERT INTO WorldUsers(WorldID,UserKey,DisplayName,CanManageTeams,CanEnableGodMode,CanAdvanceWorld) "
           "VALUES(?,'LOCAL_PRIMARY','Local player',1,1,1)",
           {c.worldId});
    const auto user = S::inserted(db);
    S::run(db, "INSERT INTO WorldUserEditModes VALUES(?,?,0,?,0)", {c.worldId, user, utc});
    const std::vector<std::string> roles = {"OWNER", "GENERAL_MANAGER", "HEAD_COACH", "OFFENSIVE_COORDINATOR",
                                            "DEFENSIVE_COORDINATOR"};
    const std::vector<std::vector<std::string>> caps = {
        {"TEAM_IDENTITY_EDIT", "VENUE_EDIT", "STAFF_ASSIGN"},
        {"DRAFT_PARTICIPATE", "FREE_AGENT_NEGOTIATE"},
        {"CALL_OFFENSIVE_PLAYS", "CALL_DEFENSIVE_PLAYS", "STAFF_ASSIGN"},
        {"CALL_OFFENSIVE_PLAYS"},
        {"CALL_DEFENSIVE_PLAYS"}};
    std::map<std::string, std::int64_t> ids, profiles;
    for (std::size_t i = 0; i < roles.size(); ++i)
    {
        const auto& role = roles[i];
        S::run(db, "INSERT INTO RoleDefinitions(WorldID,RoleKey,DisplayName) VALUES(?,?,?)",
               {c.worldId, role, role});
        ids[role] = S::inserted(db);
        S::run(db, "INSERT INTO AuthorityProfiles(WorldID,ProfileKey,ProfileVersion) VALUES(?,?,1)",
               {c.worldId, "CLASSIC_" + role});
        profiles[role] = S::inserted(db);
        for (const auto& cap : caps[i])
            S::run(db, "INSERT INTO AuthorityProfileCapabilities VALUES(?,?,?)",
                   {c.worldId, profiles.at(role), cap});
    }
    for (const auto& role : roles)
        S::run(db, "INSERT INTO AuthorityProfileStaffTargets VALUES(?,?,?)",
               {c.worldId, profiles.at("OWNER"), ids.at(role)});
    for (const auto& role : {"OFFENSIVE_COORDINATOR", "DEFENSIVE_COORDINATOR"})
        S::run(db, "INSERT INTO AuthorityProfileStaffTargets VALUES(?,?,?)",
               {c.worldId, profiles.at("HEAD_COACH"), ids.at(role)});
    for (std::size_t i = 0; i < roles.size(); ++i)
        for (std::size_t j = i + 1; j < roles.size(); ++j)
            S::run(db, "INSERT INTO RoleCompatibilityPairs VALUES(?,?,?)",
                   {c.worldId, ids.at(roles[i]), ids.at(roles[j])});
    for (const auto& pair : c.teamSeasonByCoreId)
    {
        const auto org =
            S::scalar(db, "SELECT WorldOrganizationID FROM TeamSeasons WHERE TeamSeasonID=?", {pair.second});
        for (const auto& role : roles)
        {
            S::run(db,
                   "INSERT INTO "
                   "RolePositions(WorldID,ScopeKind,WorldOrganizationID,PositionKey,RoleID,"
                   "AuthorityProfileID) VALUES(?,'ORGANIZATION',?,?,?,?)",
                   {c.worldId, org, role, ids.at(role), profiles.at(role)});
            const auto pos = S::inserted(db);
            S::Query holder(
                c.core, "SELECT CorePersonKey FROM CoreStaffAssignments WHERE TeamSeasonID=? AND RoleKey=?",
                {pair.first, role});
            if (holder.row())
            {
                const auto person = S::scalar(db,
                                              "SELECT PersonID FROM WorldPersonSourceAliases WHERE WorldID=? "
                                              "AND WorldCoreSnapshotID=? AND CorePersonKey=?",
                                              {c.worldId, c.worldCoreSnapshotId, holder.text()});
                S::run(db,
                       "INSERT INTO RoleAssignments(WorldID,PositionID,PersonID,StartsUtc,StartsRevision) "
                       "VALUES(?,?,?,?,0)",
                       {c.worldId, pos, person, utc});
            }
        }
    }
    S::run(db, "INSERT INTO WorldFeatureVersions VALUES(?,'VENUE_EVENTS',2)", {c.worldId});
    S::run(db, "INSERT INTO WorldFeatureVersions VALUES(?,'LEAGUE_WEEKS',1)", {c.worldId});
    S::run(db, "INSERT INTO WorldFeatureVersions VALUES(?,'WORLD_SCHEDULER',1)", {c.worldId});
    S::run(db, "INSERT INTO WorldEventKinds VALUES(?,'GAME_KICKOFF',1,3)", {c.worldId});
    S::run(db, "INSERT INTO WorldEventKinds VALUES(?,'VENUE_APPLY',1,1)", {c.worldId});
    S::run(db, "INSERT INTO WorldTimingPolicies VALUES(?,'VENUE_SEVEN_DAYS_V1',1,604800)", {c.worldId});
    (void)coreSeason;
    const auto vacant =
        S::scalar(db, "SELECT count(*) FROM RolePositions p WHERE NOT EXISTS(SELECT 1 FROM RoleAssignments a "
                      "WHERE a.PositionID=p.PositionID AND a.EndsUtc IS NULL)");
    if (vacant)
        c.result->diagnostics.push_back(
            {DatabaseDiagnosticSeverity::Warning,
             "CORE_IDENTITY_UNRESOLVED: " + std::to_string(vacant) +
                 " unassigned staff positions; source labels remain in the preserved Core.",
             {},
             0});
}

// The source schedule defines required fixtures, not fixed date-based closing periods.
void instantiateLeagueWeeks(CreationContext& c, const std::string& utc)
{
    namespace S = TeamDataSql;
    std::map<std::int64_t, std::vector<std::int64_t>> games;
    std::map<std::int64_t, std::pair<std::string, std::string>> times;
    S::Query source(c.world,
                    "SELECT WorldScheduledGameID,StageCode,WeekNumber,ScheduledUtc FROM WorldScheduledGames "
                    "WHERE CompetitionSeasonID=? ORDER BY WeekNumber,ScheduledUtc,WorldScheduledGameID",
                    {c.competitionSeasonId});
    bool ready = true;
    while (source.row())
    {
        const auto week = source.integer(2);
        if (source.text(1) != "REGULAR" || week <= 0 || !S::utc(source.text(3)))
        {
            ready = false;
            continue;
        }
        games[week].push_back(source.integer());
        auto& range = times[week];
        if (range.first.empty() || source.text(3) < range.first)
            range.first = source.text(3);
        if (range.second.empty() || source.text(3) > range.second)
            range.second = source.text(3);
    }
    std::int64_t previous = 0;
    std::string lastKickoff;
    for (const auto& entry : games)
    {
        if (previous && (entry.first != previous + 1 || times[entry.first].first <= lastKickoff))
            ready = false;
        previous = entry.first;
        lastKickoff = times[entry.first].second;
    }
    if (!ready || games.empty())
    {
        c.result->diagnostics.push_back(
            {DatabaseDiagnosticSeverity::Warning,
             "WEEK_STATE_UNAVAILABLE: schedule needs supported phase/week ordering and canonical kickoffs. "
             "God-mode and unrelated team edits remain available.",
             {},
             0});
        return;
    }
    const auto competition =
        S::scalar(c.world, "SELECT WorldCompetitionID FROM CompetitionSeasons WHERE CompetitionSeasonID=?",
                  {c.competitionSeasonId});
    std::vector<std::int64_t> ids;
    for (const auto& entry : games)
    {
        S::run(c.world,
               "INSERT INTO "
               "LeagueWeeks(WorldID,WorldCompetitionID,CompetitionSeasonID,WeekKey,PhaseCode,SequenceNo,"
               "DisplayWeekNumber,State) VALUES(?,?,?,?,'REGULAR',?,?,'FUTURE')",
               {c.worldId, competition, c.competitionSeasonId, "REGULAR:" + std::to_string(entry.first),
                static_cast<std::int64_t>(ids.size() + 1), entry.first});
        const auto id = S::inserted(c.world);
        ids.push_back(id);
        for (const auto game : entry.second)
            S::run(c.world, "INSERT INTO LeagueWeekGames VALUES(?,?,?,?)",
                   {c.worldId, c.competitionSeasonId, id, game});
    }
    for (std::size_t i = 1; i < ids.size(); ++i)
        S::run(c.world, "UPDATE LeagueWeeks SET NextLeagueWeekID=? WHERE WorldID=? AND LeagueWeekID=?",
               {ids[i], c.worldId, ids[i - 1]});
    S::run(
        c.world,
        "UPDATE LeagueWeeks SET State='OPEN',OpenedUtc=?,OpenedRevision=0 WHERE WorldID=? AND LeagueWeekID=?",
        {utc, c.worldId, ids.front()});
}

// Source fixture identity/time stays in WorldScheduledGames and its preserved Core.
void instantiateKickoffEvents(CreationContext &c, const std::string &utc)
{
    namespace S = TeamDataSql;
    S::Query games(c.world,
                   "SELECT WorldScheduledGameID,CompetitionSeasonID,ScheduledUtc FROM WorldScheduledGames "
                   "WHERE CompetitionSeasonID=? ORDER BY WorldScheduledGameID",
                   {c.competitionSeasonId});
    while (games.row())
    {
        if (!S::utc(games.text(2)) || games.text(2) < utc)
            throw WorldDatabaseException(
                "INVALID_TIME: fixture kickoff is missing, invalid or precedes the World start");
        S::run(c.world,
               "INSERT INTO ScheduledWorldEvents(WorldID,EventKey,KindKey,StartsUtc,EndMode,CreatedUtc,"
               "CreatedRevision,Visibility,State) VALUES(?,?,'GAME_KICKOFF',?,'DOMAIN',?,0,'PUBLIC','DRAFT')",
               {c.worldId, "kickoff:" + std::to_string(games.integer()), games.text(2), utc});
        const auto occurrence = S::inserted(c.world);
        S::run(c.world, "INSERT INTO GameKickoffOccurrences VALUES(?,?,?,?,1)",
               {c.worldId, occurrence, games.integer(1), games.integer()});
        S::run(c.world,
               "UPDATE ScheduledWorldEvents SET State='PENDING' WHERE WorldID=? AND ScheduledWorldEventID=?",
               {c.worldId, occurrence});
    }
}
