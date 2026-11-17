// Standalone regression. Arguments: schedule.pffdata, league_structure.pffdata,
// and Nfl1989RegularSeasonScores.csv. Compile with both postseason modules.
#include "Nfl1989PostseasonPairing.h"
#include "Nfl1989PostseasonSeeding.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::map<std::string, std::int64_t> historicalQualifiers()
{
    return {
        {"Denver Broncos", 1}, {"Cleveland Browns", 2}, {"Buffalo Bills", 3},
        {"Houston Oilers", 4}, {"Pittsburgh Steelers", 5}, {"San Francisco 49ers", 6},
        {"New York Giants", 7}, {"Minnesota Vikings", 8}, {"Philadelphia Eagles", 9},
        {"Los Angeles Rams", 10}
    };
}

std::vector<PostseasonPairing> loadFixtures(const char* path)
{
    const auto teams = historicalQualifiers();
    const std::map<std::string, std::int64_t> winners{
        {"WC_RAM_PHI", 10}, {"WC_PIT_HOU", 5}, {"DIV_BUF_CLE", 2},
        {"DIV_MIN_SF", 6}, {"DIV_RAM_NYG", 10}, {"DIV_PIT_DEN", 1},
        {"CONF_CLE_DEN", 1}, {"CONF_RAM_SF", 6}, {"SB_SF_DEN", 6}
    };
    const std::map<std::string, PostseasonRound> rounds{
        {"WC", PostseasonRound::WildCard}, {"DIV", PostseasonRound::Divisional},
        {"CONF", PostseasonRound::ConferenceChampionship}, {"SB", PostseasonRound::SuperBowl}
    };
    std::ifstream schedule(path);
    check(schedule.is_open(), "Cannot open 1989 schedule fixture.");
    std::vector<PostseasonPairing> fixtures;
    const std::string prefix = "[GAME_ID]1989_POST_";
    PostseasonPairing game{};
    std::string line;
    bool inGame = false;
    while (std::getline(schedule, line))
    {
        const auto start = line.find('[');
        if (start == std::string::npos)
            continue;
        const auto tag = line.substr(start);
        if (tag.compare(0, prefix.size(), prefix) == 0)
        {
            const auto id = tag.substr(prefix.size());
            game = {rounds.at(id.substr(0, id.find('_'))), 0, 0, false, winners.at(id)};
            inGame = true;
        }
        else if (inGame && tag.compare(0, 6, "[AWAY]") == 0)
            game.awayTeamSeasonId = teams.at(tag.substr(6));
        else if (inGame && tag.compare(0, 6, "[HOME]") == 0)
            game.homeTeamSeasonId = teams.at(tag.substr(6));
        else if (inGame && tag == "[SITE]NEUTRAL")
            game.neutralSite = true;
        else if (inGame && tag == "[GAME_END]")
        {
            fixtures.push_back(game);
            inGame = false;
        }
    }
    check(fixtures.size() == winners.size(), "Expected nine historical playoff fixtures.");
    return fixtures;
}

std::vector<WorldTeamRecord> loadTeams(const char* path)
{
    std::ifstream league(path);
    check(league.is_open(), "Cannot open 1989 league fixture.");
    const auto qualifiers = historicalQualifiers();
    std::vector<WorldTeamRecord> teams;
    WorldTeamRecord team;
    std::string city;
    std::int64_t nextId = 11;
    for (std::string line; std::getline(league, line);)
    {
        const auto start = line.find('[');
        if (start == std::string::npos)
            continue;
        const auto tag = line.substr(start);
        if (tag.compare(0, 12, "[CONFERENCE]") == 0)
            team.conference = tag.substr(12);
        else if (tag.compare(0, 10, "[DIVISION]") == 0)
            team.division = tag.substr(10);
        else if (tag.compare(0, 6, "[CITY]") == 0)
            city = tag.substr(6);
        else if (tag.compare(0, 6, "[TEAM]") == 0)
        {
            team.displayName = city + " " + tag.substr(6);
            const auto qualifier = qualifiers.find(team.displayName);
            team.teamSeasonId = qualifier == qualifiers.end() ? nextId++ : qualifier->second;
            teams.push_back(team);
        }
    }
    check(teams.size() == 28, "Expected 28 teams in 1989 league fixture.");
    return teams;
}

std::vector<WorldScheduleGameRecord> loadScores(const char* path,
    const std::vector<WorldTeamRecord>& teams)
{
    // Scores from https://www.jt-sw.com/football/pro/results.nsf/By/Season?OpenDocument=&Season=1989
    // matched by week and clubs to the checked-in regular-season schedule.
    std::map<std::string, std::int64_t> ids;
    for (const auto& team : teams)
        ids.emplace(team.displayName, team.teamSeasonId);
    std::ifstream scores(path);
    std::string line;
    check(static_cast<bool>(std::getline(scores, line)) && line == "away,away_score,home,home_score",
          "Cannot read 1989 scores fixture.");
    std::vector<WorldScheduleGameRecord> games;
    while (std::getline(scores, line))
    {
        std::istringstream fields(line);
        std::string away, awayScore, home, homeScore;
        check(static_cast<bool>(std::getline(fields, away, ',') &&
            std::getline(fields, awayScore, ',') && std::getline(fields, home, ',') &&
            std::getline(fields, homeScore)), "Invalid score fixture row.");
        WorldScheduleGameRecord game;
        game.scheduledGameId = static_cast<std::int64_t>(games.size()) + 1;
        game.stage = "REGULAR";
        game.status = "FINAL";
        game.hasFinalScore = true;
        game.awayTeamSeasonId = ids.at(away);
        game.homeTeamSeasonId = ids.at(home);
        game.awayScore = std::stoi(awayScore);
        game.homeScore = std::stoi(homeScore);
        games.push_back(game);
    }
    check(games.size() == 224, "Expected 224 historical regular-season results.");
    return games;
}

std::vector<PostseasonSeed> verifySeeding(const std::vector<WorldTeamRecord>& teams,
    std::vector<WorldScheduleGameRecord> games)
{
    const auto result = Nfl1989PostseasonSeeding::seed(teams, games);
    if (!result.succeeded())
        throw std::runtime_error(result.errorMessage);
    check(result.seeds.size() == 10, "Expected ten postseason seeds.");
    for (std::size_t i = 0; i < result.seeds.size(); ++i)
    {
        const auto& seed = result.seeds[i];
        // IDs 1 through 10 encode the independent historical seed table.
        check(seed.teamSeasonId == static_cast<std::int64_t>(i) + 1 &&
              seed.conference == (i < 5 ? "AFC" : "NFC") &&
              seed.seed == static_cast<int>(i % 5) + 1, "Historical seed differs.");
    }

    auto incomplete = games;
    incomplete.back().status = "SCHEDULED";
    const auto early = Nfl1989PostseasonSeeding::seed(teams, incomplete);
    check(!early.succeeded() && early.seeds.empty(), "An incomplete season produced seeds.");

    auto clubId = [&](const std::string& name)
    {
        const auto club = std::find_if(teams.begin(), teams.end(), [&](const auto& team)
        {
            return team.displayName == name;
        });
        check(club != teams.end(), "Synthetic fixture club missing.");
        return club->teamSeasonId;
    };
    auto setScore = [](auto& season, std::int64_t away, std::int64_t home, int awayScore, int homeScore)
    {
        const auto game = std::find_if(season.begin(), season.end(), [&](const auto& entry)
        {
            return entry.awayTeamSeasonId == away && entry.homeTeamSeasonId == home;
        });
        check(game != season.end(), "Synthetic fixture game missing.");
        game->awayScore = awayScore;
        game->homeScore = homeScore;
    };
    auto checkSeed = [](const Nfl1989SeedingResult& seeded, int number, std::int64_t club)
    {
        if (!seeded.succeeded())
            throw std::runtime_error(seeded.errorMessage);
        const auto& actual = seeded.seeds.at(number - 1);
        if (actual.conference != "AFC" || actual.seed != number || actual.teamSeasonId != club)
        {
            throw std::runtime_error("Wrong synthetic AFC seed " + std::to_string(number) +
                ": expected club " + std::to_string(club) + ", got " + std::to_string(actual.teamSeasonId));
        }
    };

    // Houston, Pittsburgh and the Raiders each go 6-6 in the AFC. Cincinnati
    // is 5-7 in the AFC but all four finish 9-7 overall. Houston sweeps the
    // other survivors for No. 4. Cincinnati returns and sweeps the field for No. 5.
    auto fourWildCards = games;
    const auto cincinnati = clubId("Cincinnati Bengals");
    const auto chicago = clubId("Chicago Bears");
    const auto raiders = clubId("Los Angeles Raiders");
    setScore(fourWildCards, cincinnati, chicago, 17, 14);
    setScore(fourWildCards, cincinnati, 8, 29, 21);
    setScore(fourWildCards, 2, cincinnati, 21, 14);
    const auto chiefs = clubId("Kansas City Chiefs");
    setScore(fourWildCards, cincinnati, chiefs, 17, 21);
    setScore(fourWildCards, cincinnati, raiders, 28, 7);
    setScore(fourWildCards, raiders, chiefs, 24, 19);
    setScore(fourWildCards, raiders, 7, 34, 17);
    // This margin would favor the Raiders if the head-to-head sweep were skipped.
    setScore(fourWildCards, clubId("San Diego Chargers"), raiders, 14, 70);
    const auto largerTie = Nfl1989PostseasonSeeding::seed(teams, fourWildCards);
    checkSeed(largerTie, 4, 4);
    checkSeed(largerTie, 5, cincinnati);

    // Flip Denver's two interconference wins. Denver and Buffalo now both
    // finish 9-7 and win their divisions. Denver has the head-to-head win.
    auto twoChampions = games;
    const std::int64_t denver = 1;
    auto isNfc = [&](std::int64_t id)
    {
        return std::any_of(teams.begin(), teams.end(), [&](const auto& team)
        {
            return team.teamSeasonId == id && team.conference == "NFC";
        });
    };
    for (auto& game : twoChampions)
    {
        if ((game.awayTeamSeasonId == denver && game.awayScore > game.homeScore &&
             isNfc(game.homeTeamSeasonId)) ||
            (game.homeTeamSeasonId == denver && game.homeScore > game.awayScore &&
             isNfc(game.awayTeamSeasonId)))
        {
            std::swap(game.awayScore, game.homeScore);
        }
    }
    const auto tied = Nfl1989PostseasonSeeding::seed(teams, twoChampions);
    checkSeed(tied, 1, 2);
    checkSeed(tied, 2, 1);
    checkSeed(tied, 3, 3);

    // All three champions finish 9-7. Buffalo wins on conference percentage.
    // Cleveland then takes No. 2 because it beat Denver head-to-head.
    auto threeChampions = games;
    setScore(threeChampions, clubId("San Diego Chargers"), denver, 16, 10);
    setScore(threeChampions, denver, clubId("Seattle Seahawks"), 21, 24);
    setScore(threeChampions, chiefs, 2, 11, 10);
    const auto tripleTie = Nfl1989PostseasonSeeding::seed(teams, threeChampions);
    checkSeed(tripleTie, 1, 3);
    checkSeed(tripleTie, 2, 2);
    checkSeed(tripleTie, 3, 1);
    return result.seeds;
}

bool sameGame(const PostseasonPairing& a, const PostseasonPairing& b)
{
    return a.round == b.round && a.awayTeamSeasonId == b.awayTeamSeasonId &&
           a.homeTeamSeasonId == b.homeTeamSeasonId && a.neutralSite == b.neutralSite;
}

void verify(const std::vector<PostseasonPairing>& fixtures, const std::vector<PostseasonSeed>& seeds)
{
    std::vector<PostseasonPairing> completed;
    for (const auto round : {PostseasonRound::WildCard, PostseasonRound::Divisional,
                            PostseasonRound::ConferenceChampionship, PostseasonRound::SuperBowl})
    {
        const auto generated = Nfl1989PostseasonPairing::nextRound(seeds, completed);
        const auto expectedCount = std::count_if(fixtures.begin(), fixtures.end(),
            [=](const auto& fixture) { return fixture.round == round; });
        check(generated.size() == static_cast<std::size_t>(expectedCount), "Wrong round size.");
        for (const auto& fixture : fixtures)
        {
            if (fixture.round != round)
                continue;
            check(std::count_if(generated.begin(), generated.end(),
                [&](const auto& game) { return sameGame(game, fixture) && !game.winnerTeamSeasonId; }) == 1,
                "Historical pairing differs.");
        }
        // Feed results individually, in schedule order, and require the round to wait.
        auto remaining = expectedCount;
        for (const auto& fixture : fixtures)
        {
            if (fixture.round != round)
                continue;
            completed.push_back(fixture);
            if (--remaining > 0)
            {
                const auto pending = Nfl1989PostseasonPairing::nextRound(seeds, completed);
                check(pending.size() == static_cast<std::size_t>(remaining) &&
                      std::all_of(pending.begin(), pending.end(),
                          [=](const auto& game) { return game.round == round; }),
                      "Partially completed round advanced prematurely.");
            }
        }
    }
    check(Nfl1989PostseasonPairing::nextRound(seeds, completed).empty(),
          "Postseason should end after the Super Bowl.");

    completed.push_back(completed.back());
    try
    {
        (void)Nfl1989PostseasonPairing::nextRound(seeds, completed);
    }
    catch (const std::invalid_argument&)
    {
        return;
    }
    throw std::runtime_error("Duplicate result was accepted.");
}
}

int main(int argc, char** argv)
{
    try
    {
        check(argc == 4, "Provide the 1989 schedule, league structure, and scores fixtures.");
        const auto teams = loadTeams(argv[2]);
        const auto seeds = verifySeeding(teams, loadScores(argv[3], teams));
        verify(loadFixtures(argv[1]), seeds);
        std::cout << "Ten historical seeds and nine playoff fixtures matched. "
                     "Tied champions, wild-card restarts, incomplete season, round progression, "
                     "and duplicate checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
