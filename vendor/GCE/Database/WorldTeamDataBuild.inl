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
}
void instantiateTeamData(CreationContext& c, std::int64_t coreSeason, const std::string& utc)
{
    namespace S = TeamDataSql;
    auto* db = c.world;
    S::run(db, "INSERT INTO WorldDataRevisions VALUES(?,0)", {c.worldId});
    S::run(db,
           "INSERT INTO WorldUsers(WorldID,UserKey,DisplayName,CanManageTeams,CanEnableGodMode) "
           "VALUES(?,'LOCAL_PRIMARY','Local player',1,1)",
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
    S::run(db,
           "INSERT INTO VenueHistoryBaselines SELECT WorldID,WorldVenueID,VenueName,TurfType,?,0 FROM "
           "WorldVenues WHERE WorldID=?",
           {utc, c.worldId});
    const auto comp =
        S::scalar(db, "SELECT WorldCompetitionID FROM CompetitionSeasons WHERE CompetitionSeasonID=?",
                  {c.competitionSeasonId});
    S::Query periods(
        c.core,
        "SELECT PeriodKey,CalendarKey,CalendarPolicyVersion,PhaseCode,DisplayWeekNumber,StartsUtc,EndsUtc "
        "FROM CoreLeagueGameWeekPeriods WHERE CompetitionSeasonID=? ORDER BY StartsUtc",
        {coreSeason});
    std::int64_t calendar = 0;
    std::string calendarKey, policy;
    while (periods.row())
    {
        if (!S::utc(periods.text(5)) || !S::utc(periods.text(6)) || periods.text(5) >= periods.text(6))
            throw WorldDatabaseException("Invalid canonical calendar UTC range in Core");
        if (!calendar)
        {
            calendarKey = periods.text(1);
            policy = periods.text(2);
            S::run(db,
                   "INSERT INTO LeagueGameCalendars(WorldID,WorldCompetitionID,CalendarKey,PolicyVersion) "
                   "VALUES(?,?,?,?)",
                   {c.worldId, comp, calendarKey, policy});
            calendar = S::inserted(db);
        }
        if (calendarKey != periods.text(1) || policy != periods.text(2))
            throw WorldDatabaseException(
                "CALENDAR_CONFLICT: selected season must identify one venue-history calendar/policy");
        S::Value week = periods.null(4) ? S::Value(nullptr) : S::Value(periods.integer(4));
        S::run(
            db,
            "INSERT INTO "
            "GameWeekPeriods(WorldID,CalendarID,WorldCompetitionID,CompetitionSeasonID,PeriodKey,PhaseCode,"
            "DisplayWeekNumber,StartsUtc,EndsUtc,WorldCoreSnapshotID) VALUES(?,?,?,?,?,?,?,?,?,?)",
            {c.worldId, calendar, comp, c.competitionSeasonId, periods.text(0), periods.text(3), week,
             periods.text(5), periods.text(6), c.worldCoreSnapshotId});
    }
    if (calendar)
        S::run(db,
               "INSERT INTO VenueCalendarBindings SELECT WorldID,WorldVenueID,? FROM WorldVenues WHERE "
               "WorldID=?",
               {calendar, c.worldId});
    else
        c.result->diagnostics.push_back(
            {DatabaseDiagnosticSeverity::Warning,
             "PERIOD_UNAVAILABLE: explicit calendar absent; unrelated team edits remain available.",
             {},
             0});
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
