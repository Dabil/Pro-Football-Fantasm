// Public read projection, compiled in WorldDatabase.cpp to reuse validated
// snapshot ownership and the existing Database connection infrastructure.
WorldRosterLoadResult loadWorldRoster(const std::filesystem::path& path,
    std::int64_t season, std::int64_t team)
{
    namespace S = TeamDataSql;
    WorldRosterLoadResult result;
    try
    {
        Database connection;
        if (!connection.open(path, DatabaseOpenMode::ReadOnly))
            throw WorldDatabaseException("Cannot open roster World: " + connection.lastError());
        auto* db=WorldDatabaseAccess::handle(connection);
        S::exec(db,"BEGIN");
        S::Query context(db,"SELECT t.DisplayName,w.WorldID,w.CurrentTimeUtc,s.SnapshotPath,"
            "s.SnapshotHashAlgorithm,s.SnapshotFileHash FROM TeamSeasons t "
            "JOIN CompetitionSeasons c ON c.CompetitionSeasonID=t.CompetitionSeasonID "
            "JOIN WorldCoreSnapshots s ON s.WorldCoreSnapshotID=c.WorldCoreSnapshotID "
            "JOIN Worlds w ON w.WorldID=s.WorldID WHERE t.TeamSeasonID=? AND c.CompetitionSeasonID=?",
            {team,season});
        if(!context.row()) throw WorldDatabaseException("Team is unavailable in the selected season.");
        result.teamName=context.text();
        const auto world=context.integer(1);
        const auto now=context.text(2);
        if(S::scalar(db,"SELECT count(*) FROM WorldFeatureVersions WHERE WorldID=? "
            "AND FeatureKey='ROSTER_DATA' AND ContractVersion=1",{world})!=1)
            throw WorldDatabaseException("Roster data is unavailable in this league. Build a new Core and league to enable it; this save has not been changed.");
        verifySnapshot(path,context.text(3),context.text(4),context.text(5));
        Database snapshot;
        if(!snapshot.open(resolveSnapshotPath(path,context.text(3)),DatabaseOpenMode::ReadOnly))
            throw WorldDatabaseException("Cannot open preserved roster snapshot.");
        auto* core=WorldDatabaseAccess::handle(snapshot);
        if(readCoreMetadata(core,"RosterDataContractVersion")!="1")
            throw WorldDatabaseException("Unsupported Core roster capability.");
        S::Query rows(db,"SELECT p.WorldPlayerSeasonID,p.CorePlayerKey,n.FirstName,n.LastName,"
            "p.PositionCode,p.CardType,p.JerseyNumber,r.RosterStatus,st.HealthStatus,i.GamesRemaining,"
            "d.DepthSlot FROM RosterAssignments r JOIN WorldPlayerSeasons p "
            "ON p.WorldPlayerSeasonID=r.WorldPlayerSeasonID JOIN WorldPlayers wp ON wp.WorldPlayerID=p.WorldPlayerID "
            "JOIN People n ON n.PersonID=wp.PersonID LEFT JOIN PlayerState st ON st.WorldPlayerSeasonID=p.WorldPlayerSeasonID "
            "LEFT JOIN PlayerInjuries i ON i.InjuryID=st.CurrentInjuryID AND i.WorldPlayerSeasonID=p.WorldPlayerSeasonID "
            "LEFT JOIN WorldRosterDepth d ON d.RosterAssignmentID=r.RosterAssignmentID AND d.TeamSeasonID=r.TeamSeasonID "
            "AND d.WorldPlayerSeasonID=p.WorldPlayerSeasonID AND d.PositionCode=p.PositionCode "
            "WHERE r.TeamSeasonID=? AND p.CompetitionSeasonID=? "
            "AND (r.StartDateUtc IS NULL OR r.StartDateUtc<=?) AND (r.EndDateUtc IS NULL OR r.EndDateUtc>?) "
            "ORDER BY p.PositionCode,d.DepthSlot,p.WorldPlayerSeasonID",{team,season,now,now});
        std::set<std::int64_t> seen;
        while(rows.row())
        {
            WorldRosterPlayer p;
            p.playerSeasonId=rows.integer();
            if(!seen.insert(p.playerSeasonId).second) throw WorldDatabaseException("Duplicate current roster assignment.");
            if(rows.null(8)||rows.text(8).empty()) throw WorldDatabaseException("Player availability data is missing.");
            if(rows.null(10)||rows.integer(10)<=0) throw WorldDatabaseException("Player depth assignment is missing or invalid.");
            p.firstName=rows.text(2); p.lastName=rows.text(3); p.position=rows.text(4);
            p.cardType=rows.text(5); p.jersey=static_cast<int>(rows.integer(6));
            p.rosterStatus=rows.text(7); p.healthStatus=rows.text(8); p.depth=static_cast<int>(rows.integer(10));
            if(!rows.null(9)) p.gamesRemaining=static_cast<int>(rows.integer(9));
            S::Query bio(core,"SELECT p.Age,p.HeightInches,p.WeightPounds,p.InjuryRating,d.YearsInLeague,"
                "d.ExperienceOrigin,pr.PlayerRatingProfileID FROM Players p JOIN CoreRosterData d ON d.PlayerID=p.PlayerID "
                "JOIN PlayerRatingProfiles pr ON pr.PlayerID=p.PlayerID WHERE p.SourceLocalKey=? AND pr.CardType=?",
                {rows.text(1),p.cardType});
            if(!bio.row()) throw WorldDatabaseException("Player card/experience profile is missing from preserved Core.");
            auto optionalInt=[&](int col)->std::optional<int>{return bio.null(col)?std::nullopt:std::optional<int>(static_cast<int>(bio.integer(col)));};
            p.age=optionalInt(0);p.heightInches=optionalInt(1);p.weightPounds=optionalInt(2);p.injuryRating=optionalInt(3);
            p.yearsInLeague=static_cast<int>(bio.integer(4)); p.experienceOrigin=bio.text(5);
            S::Query ratings(core,"SELECT RatingKey,RatingValue FROM PlayerRatings WHERE PlayerRatingProfileID=? ORDER BY RatingKey",{bio.integer(6)});
            while(ratings.row()) p.ratings.push_back({ratings.text(),static_cast<int>(ratings.integer(1))});
            if(p.ratings.empty()) throw WorldDatabaseException("Player rating profile is empty.");
            S::Query modifiers(db,"SELECT RatingCode,Adjustment,ReasonCode FROM PlayerRatingModifiers "
                "WHERE WorldPlayerSeasonID=? AND (EffectiveUtc IS NULL OR EffectiveUtc<=?) "
                "AND (EndUtc IS NULL OR EndUtc>?) ORDER BY ModifierID",{p.playerSeasonId,now,now});
            while(modifiers.row()) p.modifiers.push_back({modifiers.text(),modifiers.text(2),sqlite3_column_double(modifiers.statement,1)});
            result.players.push_back(std::move(p));
        }
        result.success=true;
    }
    catch(const std::exception& e) { result.players.clear();result.errorMessage=e.what(); }
    return result;
}
