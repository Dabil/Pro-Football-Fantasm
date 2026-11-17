#pragma once

#include "Nfl1989PostseasonSeeding.h"
#include "GCE/Database/TeamDataSql.h"
#include "GCE/Database/WorldTeamData.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

// Called after the game result has committed. Keep fixture publication atomic
// and independent of that save so a calendar error cannot undo a final score.
inline void scheduleNfl1989PostseasonRound(const std::filesystem::path& path,
                                          std::int64_t season, WorldEditCommit& commit)
{
    namespace S = TeamDataSql;
    sqlite3* raw = nullptr;
    const auto opened = sqlite3_open_v2(path.u8string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(raw, sqlite3_close);
    if (opened != SQLITE_OK)
        throw std::runtime_error("Cannot open the World calendar.");
    auto* db = connection.get();
    sqlite3_busy_timeout(db, 3000);
    S::exec(db, "PRAGMA foreign_keys=ON");
    if (!S::scalar(db, "SELECT count(*) FROM CompetitionSeasons s JOIN WorldCompetitions c "
        "ON c.WorldCompetitionID=s.WorldCompetitionID JOIN Worlds w ON w.WorldID=c.WorldID "
        "WHERE s.CompetitionSeasonID=? AND s.CompetitionPolicyProfileKey='NFL_1989_POSTSEASON_V1' "
        "AND s.SeasonYear=1989 AND s.Status='ACTIVE' AND w.WorldID=? AND w.WorldKey=?",
        {season, commit.worldId, commit.worldKey}))
        return;

    S::Transaction transaction(db);
    const auto world = commit.worldId;
    if (!S::scalar(db, "SELECT count(*) FROM WorldUsers WHERE WorldID=? AND UserKey='LOCAL_PRIMARY' "
                       "AND Active=1 AND CanAdvanceWorld=1", {world}))
        return;
    if (S::scalar(db, "SELECT count(*) FROM WorldScheduledGames s "
        "WHERE s.CompetitionSeasonID=? AND s.StageCode IN ('REGULAR','POSTSEASON') AND NOT EXISTS "
        "(SELECT 1 FROM FinalizedLeagueGameResults f WHERE f.WorldID=? "
        "AND f.WorldScheduledGameID=s.WorldScheduledGameID)", {season, world}))
        return;

    static const std::array<std::string,4> roundCodes{
        "WILD_CARD","DIVISIONAL","CONFERENCE_CHAMPIONSHIP","SUPER_BOWL"};
    std::vector<WorldScheduleGameRecord> schedule;
    S::Query scores(db, "SELECT s.WorldScheduledGameID,s.AwayTeamSeasonID,s.HomeTeamSeasonID,"
        "g.AwayScore,g.HomeScore,g.GameStatus,f.CompletedUtc,s.StageCode,s.RoundCode,s.SiteCode,"
        "g.WinnerTeamSeasonID FROM WorldScheduledGames s "
        "JOIN FinalizedLeagueGameResults f ON f.WorldScheduledGameID=s.WorldScheduledGameID "
        "AND f.WorldID=? JOIN Games g ON g.GameID=f.GameID AND g.WorldID=f.WorldID "
        "WHERE s.CompetitionSeasonID=? AND s.StageCode IN ('REGULAR','POSTSEASON')", {world, season});
    std::string completedAt;
    std::vector<PostseasonPairing> completed;
    while (scores.row())
    {
        completedAt = std::max(completedAt, scores.text(6));
        if (scores.text(7) == "POSTSEASON")
        {
            const auto code = scores.text(8);
            const auto round = std::find(roundCodes.begin(),roundCodes.end(),code);
            if (round == roundCodes.end() || scores.text(5) != "FINAL" || scores.null(10))
                throw std::runtime_error("A completed playoff game has no valid round or winner.");
            completed.push_back({static_cast<PostseasonRound>(round-roundCodes.begin()),
                scores.integer(1),scores.integer(2),scores.text(9)=="NEUTRAL",scores.integer(10)});
            continue;
        }
        WorldScheduleGameRecord game;
        game.scheduledGameId = scores.integer();
        game.awayTeamSeasonId = scores.integer(1);
        game.homeTeamSeasonId = scores.integer(2);
        game.awayScore = static_cast<int>(scores.integer(3));
        game.homeScore = static_cast<int>(scores.integer(4));
        game.hasFinalScore = !scores.null(3) && !scores.null(4);
        game.status = scores.text(5);
        game.stage = "REGULAR";
        schedule.push_back(std::move(game));
    }
    const auto teams = loadWorldTeams(path, season);
    if (!teams.succeeded())
        throw std::runtime_error(teams.errorMessage);
    const auto seeded = Nfl1989PostseasonSeeding::seed(teams.teams, schedule);
    if (!seeded.succeeded())
        throw std::runtime_error(seeded.errorMessage);
    const auto pairings = Nfl1989PostseasonPairing::nextRound(seeded.seeds,completed);
    if (pairings.empty())
        return;
    const auto round = pairings.front().round;
    const auto& roundCode = roundCodes.at(static_cast<std::size_t>(round));
    const std::size_t expected = round == PostseasonRound::Divisional ? 4 :
        round == PostseasonRound::SuperBowl ? 1 : 2;
    if (pairings.size() != expected)
        throw std::runtime_error("The playoff calendar has an incomplete set of round results.");

    S::Query state(db, "SELECT r.Revision,w.CurrentTimeUtc FROM WorldDataRevisions r "
        "JOIN Worlds w ON w.WorldID=r.WorldID WHERE w.WorldID=?", {world});
    if (!state.row())
        throw std::runtime_error("The World clock is unavailable.");
    const auto revision = state.integer();
    const auto currentTime = state.text(1);
    if (revision == std::numeric_limits<std::int64_t>::max())
        throw std::runtime_error("World revision exhausted.");
    const auto nextRevision = revision + 1;
    S::run(db, "INSERT INTO LeagueWeeks(WorldID,WorldCompetitionID,CompetitionSeasonID,WeekKey,PhaseCode,"
        "SequenceNo,State) SELECT ?,WorldCompetitionID,CompetitionSeasonID,"
        "?,'POSTSEASON',"
        "(SELECT COALESCE(max(SequenceNo),0)+1 FROM LeagueWeeks WHERE CompetitionSeasonID=?),"
        "'FUTURE' FROM CompetitionSeasons WHERE CompetitionSeasonID=?",
        {world, "POSTSEASON_"+roundCode, season, season});
    const auto week = S::inserted(db);

    for (const auto& pairing : pairings)
    {
        const auto home = std::find_if(seeded.seeds.begin(), seeded.seeds.end(), [&](const auto& seed)
        { return seed.teamSeasonId == pairing.homeTeamSeasonId; });
        std::string key = "PFF_NFL_1989_";
        switch(round)
        {
        case PostseasonRound::WildCard: key += "WC_" + home->conference; break;
        case PostseasonRound::Divisional:
            // In the authored 1989 calendar, the #1 seed hosts in AFC slot 2
            // and NFC slot 1. The other slot belongs to the #2 seed.
            key += "DIV_" + home->conference + ((home->seed == 1) == (home->conference == "NFC") ? "_1" : "_2");
            break;
        case PostseasonRound::ConferenceChampionship: key += "CONF_" + home->conference; break;
        case PostseasonRound::SuperBowl: key += "SUPER_BOWL"; break;
        }
        S::Query slot(db, "SELECT e.ScheduledWorldEventID,e.StartsUtc,d.LocalDateTime,"
            "strftime('%w',d.LocalDateTime),COALESCE(v.WorldVenueID,0) FROM SeasonEventDefinitions d "
            "JOIN SeasonEventInstances i ON i.WorldID=d.WorldID AND i.DefinitionID=d.DefinitionID "
            "JOIN ScheduledWorldEvents e ON e.WorldID=i.WorldID AND e.ScheduledWorldEventID=i.ScheduledWorldEventID "
            "LEFT JOIN WorldVenues v ON v.WorldID=i.WorldID AND i.InstanceScopeKey='VENUE:'||v.WorldVenueID "
            "WHERE d.WorldID=? AND d.CompetitionSeasonID=? AND d.EventKey=? AND d.EventType='POSTSEASON_SLOT' "
            "AND d.PolicyKey='NFL_1989_POSTSEASON_V1' AND d.PolicyVersion=1 AND e.State='PENDING'",
            {world, season, key});
        if (!slot.row() || slot.text(1) < completedAt || slot.text(1) < currentTime)
            throw std::runtime_error("Missing or elapsed postseason kickoff slot: " + key);
        const auto reservation = slot.integer();
        const auto kickoff = slot.text(1);
        const auto local = slot.text(2);
        static const char* const days[]{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        const std::string day = days[std::stoi(slot.text(3))];
        const int hour = std::stoi(local.substr(11,2));
        const std::string time = std::to_string(hour % 12 ? hour % 12 : 12) +
            local.substr(13,3) + (hour < 12 ? "AM" : "PM");
        const auto venue = pairing.neutralSite ? slot.integer(4) : S::scalar(db, "SELECT COALESCE(WorldVenueID,0) FROM TeamSeasons "
            "WHERE TeamSeasonID=? AND CompetitionSeasonID=?", {pairing.homeTeamSeasonId, season});
        if (!venue)
            throw std::runtime_error("The postseason matchup has no venue: " + key);
        S::run(db, "INSERT INTO WorldScheduledGames(CompetitionSeasonID,ScheduleKey,StageCode,RoundCode,"
            "GameDateLabel,DayLabel,TimeLabel,ScheduledUtc,AwayTeamSeasonID,HomeTeamSeasonID,WorldVenueID,"
            "SiteCode,Status) VALUES(?,?,'POSTSEASON',?,?,?,?,?,?,?,?,?,'SCHEDULED')",
            {season, key, roundCode, local.substr(0,10), day, time, kickoff,
             pairing.awayTeamSeasonId, pairing.homeTeamSeasonId, venue,
             std::string(pairing.neutralSite ? "NEUTRAL" : "HOME")});
        const auto game = S::inserted(db);
        S::run(db, "INSERT INTO LeagueWeekGames VALUES(?,?,?,?)", {world, season, week, game});
        S::run(db, "UPDATE ScheduledWorldEvents SET State='SUPERSEDED',ResolvedUtc=?,ResolvedRevision=?,"
            "ResolutionReason='POSTSEASON_PAIRED' WHERE WorldID=? AND ScheduledWorldEventID=?",
            {completedAt, nextRevision, world, reservation});
        S::run(db, "INSERT INTO ScheduledWorldEvents(WorldID,EventKey,KindKey,StartsUtc,EndMode,CreatedUtc,"
            "CreatedRevision,Visibility,State,SupersedesEventID) VALUES(?,?,'GAME_KICKOFF',?,'DOMAIN',?,?,'PUBLIC','DRAFT',?)",
            {world, "kickoff:"+std::to_string(game), kickoff, completedAt, nextRevision, reservation});
        const auto event = S::inserted(db);
        S::run(db, "INSERT INTO GameKickoffOccurrences VALUES(?,?,?,?,1)", {world, event, season, game});
        S::run(db, "UPDATE ScheduledWorldEvents SET State='PENDING' WHERE WorldID=? AND ScheduledWorldEventID=?",
            {world, event});
    }
    S::run(db, "UPDATE LeagueWeeks SET State='OPEN',OpenedUtc=?,OpenedRevision=? WHERE LeagueWeekID=?",
        {completedAt, nextRevision, week});
    S::run(db, "UPDATE CompetitionSeasons SET CurrentPhaseCode='POSTSEASON' WHERE CompetitionSeasonID=?", {season});
    S::run(db, "UPDATE WorldDataRevisions SET Revision=? WHERE WorldID=?", {nextRevision, world});
    transaction.commit();
    commit.revision = commit.committedRevision = nextRevision;
    commit.currentTimeUtc = currentTime;
    commit.changed = true;
}
