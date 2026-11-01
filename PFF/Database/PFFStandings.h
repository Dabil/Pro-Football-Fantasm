#pragma once

#include "GCE/Database/WorldDatabase.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Read-only Classic table projection. The World API owns identity, fixture
// ordering and information visibility. This does not rank tied teams, determine
// playoff eligibility or mutate/cache standings in the saved World.
struct PffStandingRow
{
    WorldTeamRecord team;
    std::int64_t wins = 0;
    std::int64_t losses = 0;
    std::int64_t ties = 0;
    std::int64_t pointsFor = 0;
    std::int64_t pointsAgainst = 0;
    char streakResult = '-';
    std::int64_t streakLength = 0;

    double winPercentage() const noexcept
    {
        const auto played = wins + losses + ties;
        return played == 0 ? 0.0 :
            (static_cast<double>(wins) + static_cast<double>(ties) * 0.5) / played;
    }
};

struct PffStandingsResult
{
    bool success = false;
    std::vector<PffStandingRow> rows;
    std::string errorMessage;
};

inline PffStandingsResult projectPffStandings(
    const std::vector<WorldTeamRecord>& teams,
    const std::vector<WorldScheduleGameRecord>& chronologicalVisibleGames)
{
    PffStandingsResult result;
    std::map<std::int64_t, std::size_t> teamRows;
    for (const auto& team : teams)
    {
        teamRows.emplace(team.teamSeasonId, result.rows.size());
        PffStandingRow row;
        row.team = team;
        result.rows.push_back(std::move(row));
    }

    std::set<std::int64_t> countedFixtures;
    for (const auto& game : chronologicalVisibleGames)
    {
        if (game.stage != "REGULAR" || game.status != "FINAL")
        {
            continue;
        }

        const auto away = teamRows.find(game.awayTeamSeasonId);
        const auto home = teamRows.find(game.homeTeamSeasonId);
        if (!game.hasFinalScore || game.awayScore < 0 || game.homeScore < 0 ||
            away == teamRows.end() || home == teamRows.end() || away == home ||
            !countedFixtures.insert(game.scheduledGameId).second)
        {
            result.rows.clear();
            result.errorMessage = "A completed regular-season game has missing or inconsistent results.";
            return result;
        }

        auto record = [](PffStandingRow& row, int scored, int conceded)
        {
            row.pointsFor += scored;
            row.pointsAgainst += conceded;
            const char outcome = scored > conceded ? 'W' : scored < conceded ? 'L' : 'T';
            if (outcome == 'W') ++row.wins;
            else if (outcome == 'L') ++row.losses;
            else ++row.ties;
            row.streakLength = row.streakResult == outcome ? row.streakLength + 1 : 1;
            row.streakResult = outcome;
        };
        record(result.rows[away->second], game.awayScore, game.homeScore);
        record(result.rows[home->second], game.homeScore, game.awayScore);
    }
    result.success = true;
    return result;
}

inline PffStandingsResult loadPffStandings(
    const std::filesystem::path& databasePath, std::int64_t competitionSeasonId)
{
    const auto teams = loadWorldTeams(databasePath, competitionSeasonId);
    if (!teams.succeeded()) return { false, {}, teams.errorMessage };
    const auto schedule = loadWorldCompetitionSchedule(databasePath, competitionSeasonId);
    if (!schedule.succeeded()) return { false, {}, schedule.errorMessage };
    return projectPffStandings(teams.teams, schedule.games);
}
