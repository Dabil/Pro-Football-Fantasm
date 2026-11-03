// Included inside the Core builder's private namespace, after its importers.
void buildCoreTeamData(BuildContext& context, const std::map<std::string, std::filesystem::path>& selected,
                       const std::filesystem::path& league)
{
    namespace S = TeamDataSql;
    auto* db = context.db;
    const std::map<std::string, std::string> roles = {{"OWNER", "OWNER"},
                                                      {"GM", "GENERAL_MANAGER"},
                                                      {"HEADCOACH", "HEAD_COACH"},
                                                      {"OFFENSIVE_COORDINATOR", "OFFENSIVE_COORDINATOR"},
                                                      {"DEFENSIVE_COORDINATOR", "DEFENSIVE_COORDINATOR"}};
    for (const auto& role : roles)
        S::run(db, "INSERT INTO CoreStaffRoles VALUES(?,?)", {role.second, role.second});
    auto sourceId = [&](const std::filesystem::path& file) {
        return S::scalar(db, "SELECT SourceFileID FROM SourceFiles WHERE RelativePath=?",
                         {pathUtf8(std::filesystem::relative(file, context.sourceRoot))});
    };
    // Keep unresolved source labels without inventing People from display names.
    std::string city, nickname, key;
    std::vector<TagLine> labels;
    bool inTeam = false;
    for (const auto& t : readTaggedFile(league))
    {
        if (t.tag == "TEAM_BEGIN")
        {
            inTeam = true;
            city.clear();
            nickname.clear();
            key.clear();
            labels.clear();
        }
        else if (t.tag == "CITY")
            city = t.value;
        else if (t.tag == "TEAM")
            nickname = t.value;
        else if (t.tag == "TEAM_SEASON_KEY")
            key = t.value;
        else if (roles.count(t.tag))
            labels.push_back(t);
        else if (t.tag == "TEAM_END" && inTeam)
        {
            const auto id = key.empty() ? context.teamSeasonBySourceName.at(sourceTeamNameKey(city, nickname))
                                        : context.teamSeasonByKey.at(key);
            for (const auto& label : labels)
            {
                try
                {
                    S::run(db, "INSERT INTO CoreStaffSourceLabels VALUES(?,?,?,?,?)",
                           {id, roles.at(label.tag), label.value, sourceId(league),
                            static_cast<std::int64_t>(label.line)});
                }
                catch (const std::exception& e)
                {
                    throw BuildException(std::string(e.what()) + " team=" + key + " role=" + label.tag,
                                         league, label.line);
                }
            }
            inTeam = false;
        }
    }
    struct Record
    {
        std::string kind;
        std::map<std::string, TagLine> fields;
        std::size_t line = 0;
    };
    auto records = [&](const std::string& name, const std::map<std::string, std::set<std::string>>& shapes) {
        std::vector<Record> out;
        const auto f = selected.find(name);
        if (f == selected.end())
            return out;
        Record record;
        for (const auto& tag : readTaggedFile(f->second))
        {
            const auto begin = tag.tag.size() > 6 && tag.tag.substr(tag.tag.size() - 6) == "_BEGIN";
            if (begin)
            {
                if (!record.kind.empty())
                    throw BuildException("Nested TeamData source record", f->second, tag.line);
                record = {tag.tag.substr(0, tag.tag.size() - 6), {}, tag.line};
                if (!shapes.count(record.kind) || !tag.value.empty())
                    throw BuildException("Unsupported TeamData record", f->second, tag.line);
            }
            else if (tag.tag == record.kind + "_END" && !record.kind.empty())
            {
                if (!tag.value.empty())
                    throw BuildException("Unexpected end value", f->second, tag.line);
                out.push_back(record);
                record = {};
            }
            else
            {
                if (record.kind.empty() || !shapes.at(record.kind).count(tag.tag) ||
                    !record.fields.emplace(tag.tag, tag).second || tag.value.empty())
                    throw BuildException("Unexpected, duplicate or empty TeamData field: " + tag.tag,
                                         f->second, tag.line);
            }
        }
        if (!record.kind.empty())
            throw BuildException("Unclosed TeamData source record", f->second, record.line);
        return out;
    };
    auto field = [](const Record& r, const char* key) -> std::string {
        const auto i = r.fields.find(key);
        if (i == r.fields.end())
            throw BuildException(std::string("Missing TeamData field ") + key, {}, r.line);
        return i->second.value;
    };
    auto optional = [](const Record& r, const char* key) -> S::Value {
        auto i = r.fields.find(key);
        return i == r.fields.end() ? S::Value(nullptr) : S::Value(i->second.value);
    };
    auto prepare = [&](const char* name, const char* family) {
        auto f = selected.find(name);
        if (f == selected.end())
            return;
        registerSourceFile(context, f->second, family, "IMPORTED", 0);
        ++context.result->filesProcessed;
    };
    prepare("people.pffdata", "PEOPLE");
    prepare("staff_assignments.pffdata", "STAFF_ASSIGNMENTS");
    prepare("game_week_periods.pffdata", "GAME_WEEK_PERIODS");
    const auto people =
        records("people.pffdata",
                {{"PERSON", {"PERSON_KEY", "DISPLAY_NAME", "FIRST_NAME", "LAST_NAME", "BIRTH_DATE"}},
                 {"PLAYER_PERSON", {"PLAYER_SOURCE_KEY", "PERSON_KEY"}}});
    for (const auto& r : people)
        if (r.kind == "PERSON")
        {
            if (r.fields.count("BIRTH_DATE") && !S::utc(r.fields.at("BIRTH_DATE").value + "T00:00:00Z"))
                throw BuildException("Invalid birth date", selected.at("people.pffdata"), r.line);
            S::run(db, "INSERT INTO CorePeople VALUES(?,?,?,?,?,'EXPLICIT',?,?)",
                   {field(r, "PERSON_KEY"), optional(r, "FIRST_NAME"), optional(r, "LAST_NAME"),
                    field(r, "DISPLAY_NAME"), optional(r, "BIRTH_DATE"),
                    sourceId(selected.at("people.pffdata")), static_cast<std::int64_t>(r.line)});
            context.inserted();
        }
    for (const auto& r : people)
        if (r.kind == "PLAYER_PERSON")
        {
            const auto id = S::scalar(db, "SELECT PlayerID FROM Players WHERE SourceLocalKey=?",
                                      {field(r, "PLAYER_SOURCE_KEY")});
            S::run(db, "INSERT INTO CorePlayerPeople VALUES(?,?)", {id, field(r, "PERSON_KEY")});
            context.inserted();
        }
    S::exec(db, "INSERT INTO CorePeople(CorePersonKey,FirstName,LastName,DisplayName,IdentityStatus) SELECT "
                "'legacy-player:'||p.SourceLocalKey,p.FirstName,p.LastName,trim(p.FirstName||' "
                "'||p.LastName),'LEGACY_PLAYER_BRIDGE' FROM Players p LEFT JOIN CorePlayerPeople m ON "
                "m.PlayerID=p.PlayerID WHERE m.PlayerID IS NULL; INSERT INTO CorePlayerPeople SELECT "
                "p.PlayerID,'legacy-player:'||p.SourceLocalKey FROM Players p LEFT JOIN CorePlayerPeople m "
                "ON m.PlayerID=p.PlayerID WHERE m.PlayerID IS NULL;");
    for (const auto& r : records("staff_assignments.pffdata",
                                 {{"STAFF_ASSIGNMENT", {"TEAM_SEASON_KEY", "ROLE_KEY", "PERSON_KEY"}}}))
    {
        const auto team = S::scalar(db, "SELECT TeamSeasonID FROM TeamSeasons WHERE TeamSeasonKey=?",
                                    {field(r, "TEAM_SEASON_KEY")});
        S::run(db,
               "INSERT INTO CoreStaffAssignments(TeamSeasonID,RoleKey,CorePersonKey,SourceFileID,SourceLine) "
               "VALUES(?,?,?,?,?)",
               {team, field(r, "ROLE_KEY"), field(r, "PERSON_KEY"),
                sourceId(selected.at("staff_assignments.pffdata")), static_cast<std::int64_t>(r.line)});
        context.inserted();
    }
    // Leadership jobs may be combined without a role-count limit at the same organization.
    if (S::scalar(
            db,
            R"SQL(SELECT count(*) FROM CoreStaffAssignments a JOIN CoreStaffAssignments b ON a.CorePersonKey=b.CorePersonKey AND a.CoreStaffAssignmentID<b.CoreStaffAssignmentID
        WHERE a.TeamSeasonID<>b.TeamSeasonID)SQL"))
        throw BuildException("ROLE_INCOMPATIBLE: concurrent source jobs must belong to the same organization");
    for (const auto& r : records("game_week_periods.pffdata",
                                 {{"GAME_WEEK",
                                   {"PERIOD_KEY", "SEASON_KEY", "CALENDAR_KEY", "CALENDAR_POLICY_VERSION",
                                    "PHASE", "DISPLAY_WEEK", "STARTS_UTC", "ENDS_UTC"}}}))
    {
        const auto start = field(r, "STARTS_UTC"), end = field(r, "ENDS_UTC");
        if (!S::utc(start) || !S::utc(end) || start >= end)
            throw BuildException("Invalid game-week UTC range", selected.at("game_week_periods.pffdata"),
                                 r.line);
        auto season =
            S::scalar(db,
                      "SELECT cs.CompetitionSeasonID FROM CompetitionSeasons cs JOIN Competitions c ON "
                      "c.CompetitionID=cs.CompetitionID WHERE c.CompetitionKey||'_'||cs.SeasonYear=?",
                      {field(r, "SEASON_KEY")});
        S::Value week = nullptr;
        if (r.fields.count("DISPLAY_WEEK"))
            week = static_cast<std::int64_t>(parseInt(
                field(r, "DISPLAY_WEEK"), selected.at("game_week_periods.pffdata"), r.line, "DISPLAY_WEEK"));
        if (field(r, "PHASE") == "REGULAR" &&
            (std::holds_alternative<std::nullptr_t>(week) || std::get<std::int64_t>(week) < 1))
            throw BuildException("Regular period requires positive DISPLAY_WEEK");
        S::run(db, "INSERT INTO CoreLeagueGameWeekPeriods VALUES(?,?,?,?,?,?,?,?,?,?)",
               {field(r, "PERIOD_KEY"), season, field(r, "CALENDAR_KEY"), field(r, "CALENDAR_POLICY_VERSION"),
                field(r, "PHASE"), week, start, end, sourceId(selected.at("game_week_periods.pffdata")),
                static_cast<std::int64_t>(r.line)});
        context.inserted();
    }
    for (const auto& name : {"people.pffdata", "staff_assignments.pffdata", "game_week_periods.pffdata"})
        if (selected.count(name))
        {
            const auto tags = readTaggedFile(selected.at(name));
            const auto count = std::count_if(tags.begin(), tags.end(), [](const auto& t) {
                return t.tag.size() > 6 && t.tag.substr(t.tag.size() - 6) == "_BEGIN";
            });
            S::run(db, "UPDATE SourceFiles SET RecordsImported=? WHERE SourceFileID=?",
                   {static_cast<std::int64_t>(count), sourceId(selected.at(name))});
        }
    const auto unresolved = S::scalar(
        db, "SELECT count(*) FROM CoreStaffSourceLabels l LEFT JOIN CoreStaffAssignments a ON "
            "a.TeamSeasonID=l.TeamSeasonID AND a.RoleKey=l.RoleKey WHERE a.CoreStaffAssignmentID IS NULL");
    if (unresolved)
        context.diagnostic(DatabaseDiagnosticSeverity::Warning,
                           "CORE_IDENTITY_UNRESOLVED: " + std::to_string(unresolved) +
                               " leadership labels preserved without invented People.");
    if (!S::scalar(db, "SELECT count(*) FROM CoreLeagueGameWeekPeriods"))
        context.diagnostic(DatabaseDiagnosticSeverity::Warning,
                           "PERIOD_UNAVAILABLE: no explicit league game-week calendar; venue history editing "
                           "is unavailable until calendar data is installed.");
    insertMetadata(db, context, "TeamDataContractVersion", "1");
    insertMetadata(db, context, "CompilerBuildId", "TEAMDATA-STAGE2-2026-11-03");
    insertMetadata(db, context, "PersonIdentityPolicy", "EXPLICIT_OR_LEGACY_PLAYER_BRIDGE_V1");
    insertMetadata(db, context, "StaffUnresolvedLabelCount", std::to_string(unresolved));
    insertMetadata(db, context, "StaffAssignmentCount",
                   std::to_string(S::scalar(db, "SELECT count(*) FROM CoreStaffAssignments")));
    S::Query policies(db, "SELECT DISTINCT CalendarKey,CalendarPolicyVersion FROM CoreLeagueGameWeekPeriods "
                          "ORDER BY CalendarKey,CalendarPolicyVersion");
    std::string policyIdentity;
    while (policies.row())
    {
        if (!policyIdentity.empty())
            policyIdentity += ";";
        policyIdentity += policies.text(0) + "@" + policies.text(1);
    }
    insertMetadata(db, context, "CalendarPolicyIdentity",
                   policyIdentity.empty() ? "UNKNOWN_NOT_INSTALLED" : policyIdentity);
}
