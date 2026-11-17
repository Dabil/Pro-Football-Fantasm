#include "Nfl1989PostseasonSeeding.h"
#include "PFFStandings.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
using Id = std::int64_t;
using Game = WorldScheduleGameRecord;
using Row = PffStandingRow;
using Group = std::vector<const Row*>;

struct Ratio
{
    Id numerator = 0;
    Id denominator = 1;
};

bool operator<(Ratio a, Ratio b)
{
    return a.numerator * b.denominator < b.numerator * a.denominator;
}

Ratio record(const Row& row)
{
    return {2 * row.wins + row.ties, 2 * (row.wins + row.losses + row.ties)};
}

template <class Metric>
Group leaders(const Group& group, Metric metric)
{
    Group best;
    std::optional<Ratio> highest;
    for (const auto* row : group)
    {
        const auto value = metric(*row);
        if (!value)
            return group; // The criterion must be applicable to every club.
        if (!highest || *highest < *value)
        {
            best.clear();
            highest = value;
        }
        if (!(*value < *highest))
            best.push_back(row);
    }
    return best;
}

struct ClubGame
{
    const WorldTeamRecord* opponent;
    Id forPoints;
    Id againstPoints;
};

struct GameTotals
{
    Id games = 0;
    Id halfWins = 0;
    Id margin = 0;

    std::optional<Ratio> percentage(Id minimum = 1) const
    {
        if (games < minimum)
            return std::nullopt;
        return Ratio{halfWins, 2 * games};
    }
};

using ClubGames = std::map<Id, std::vector<ClubGame>>;

template <class Predicate>
GameTotals totals(const ClubGames& games, const Row& row, Predicate includes)
{
    GameTotals result;
    for (const auto& game : games.at(row.team.teamSeasonId))
    {
        if (!includes(*game.opponent))
            continue;
        ++result.games;
        result.margin += game.forPoints - game.againstPoints;
        result.halfWins += game.forPoints > game.againstPoints ? 2 :
                           game.forPoints == game.againstPoints ? 1 : 0;
    }
    return result;
}

bool sameDivision(const WorldTeamRecord& a, const WorldTeamRecord& b)
{
    return a.conference == b.conference && a.division == b.division;
}

std::set<Id> commonOpponents(const ClubGames& games, const Group& group)
{
    auto opponents = [&](const Row& row)
    {
        std::set<Id> ids;
        for (const auto& game : games.at(row.team.teamSeasonId))
            ids.insert(game.opponent->teamSeasonId);
        return ids;
    };
    auto common = opponents(*group.front());
    for (auto it = std::next(group.begin()); it != group.end(); ++it)
    {
        const auto next = opponents(**it);
        std::set<Id> intersection;
        std::set_intersection(common.begin(), common.end(), next.begin(), next.end(),
                              std::inserter(intersection, intersection.end()));
        common = std::move(intersection);
    }
    return common;
}

enum class Criterion { HeadToHead, DivisionRecord, ConferenceRecord, CommonRecord,
                       DivisionMargin, ConferenceAverageMargin, OverallMargin };

const Row* breakTie(const ClubGames& games, const Group& group, bool forWildCard = false)
{
    const bool division = std::all_of(group.begin(), group.end(), [&](const Row* row)
    {
        return sameDivision(row->team, group.front()->team);
    });
    const std::vector<Criterion> criteria = division ?
        std::vector<Criterion>{Criterion::HeadToHead, Criterion::DivisionRecord,
            Criterion::ConferenceRecord, Criterion::CommonRecord, Criterion::DivisionMargin,
            Criterion::OverallMargin} :
        std::vector<Criterion>{Criterion::HeadToHead, Criterion::ConferenceRecord,
            Criterion::CommonRecord, Criterion::ConferenceAverageMargin, Criterion::OverallMargin};

    for (const auto criterion : criteria)
    {
        const auto common = criterion == Criterion::CommonRecord ?
            commonOpponents(games, group) : std::set<Id>{};
        const auto best = leaders(group, [&](const Row& row) -> std::optional<Ratio>
        {
            switch (criterion)
            {
            case Criterion::HeadToHead:
            {
                if (!division && group.size() > 2)
                {
                    bool swept = true;
                    bool sweptByAll = true;
                    for (const auto* opponent : group)
                    {
                        if (opponent == &row)
                            continue;
                        const auto against = totals(games, row, [&](const auto& team)
                        {
                            return team.teamSeasonId == opponent->team.teamSeasonId;
                        });
                        swept = swept && against.games > 0 && against.halfWins == 2 * against.games;
                        sweptByAll = sweptByAll && against.games > 0 && against.halfWins == 0;
                    }
                    return Ratio{swept ? 1 : sweptByAll ? -1 : 0, 1};
                }
                return totals(games, row, [&](const auto& opponent)
                {
                    return std::any_of(group.begin(), group.end(), [&](const Row* other)
                    {
                        return opponent.teamSeasonId == other->team.teamSeasonId;
                    });
                }).percentage();
            }
            case Criterion::DivisionRecord:
            case Criterion::DivisionMargin:
            {
                const auto within = totals(games, row, [&](const auto& opponent)
                {
                    return sameDivision(row.team, opponent);
                });
                if (criterion == Criterion::DivisionRecord)
                    return within.percentage();
                return Ratio{within.margin, 1};
            }
            case Criterion::ConferenceRecord:
            case Criterion::ConferenceAverageMargin:
            {
                const auto within = totals(games, row, [&](const auto& opponent)
                {
                    return row.team.conference == opponent.conference;
                });
                if (criterion == Criterion::ConferenceRecord)
                    return within.percentage();
                if (within.games == 0)
                    return std::nullopt;
                return Ratio{within.margin, within.games};
            }
            case Criterion::CommonRecord:
                return totals(games, row, [&](const auto& opponent)
                {
                    return common.count(opponent.teamSeasonId) != 0;
                }).percentage(division ? 1 : 4);
            case Criterion::OverallMargin:
                return Ratio{row.pointsFor - row.pointsAgainst, 1};
            }
            return std::nullopt;
        });
        if (best.size() == 1)
            return best.front();
        const bool separatedPair = best.size() == 2 && group.size() > 2;
        const bool fourToThree = forWildCard && group.size() == 4 && best.size() == 3;
        if (separatedPair || fourToThree)
            return breakTie(games, best, forWildCard);
        if (best.size() < group.size())
            throw std::domain_error("NFL89-SEED-OPEN-002: larger-group restart procedure is unresolved.");
    }
    throw std::domain_error("1989 tiebreak reached strength of schedule, touchdown differential, or coin toss. Later criteria are unavailable.");
}

Group topRecord(const Group& group)
{
    return leaders(group, [](const Row& row) -> std::optional<Ratio> { return record(row); });
}

void remove(Group& group, const Row* row)
{
    group.erase(std::find(group.begin(), group.end(), row));
}
}

Nfl1989SeedingResult Nfl1989PostseasonSeeding::seed(
    const std::vector<WorldTeamRecord>& teams, const std::vector<Game>& schedule)
{
    std::map<Id, const WorldTeamRecord*> teamById;
    for (const auto& team : teams)
    {
        if (team.teamSeasonId <= 0 || (team.conference != "AFC" && team.conference != "NFC") ||
            team.division.empty() || !teamById.emplace(team.teamSeasonId, &team).second)
        {
            return {{}, "Invalid team identity or group in 1989 seeding input."};
        }
    }
    for (const auto& game : schedule)
    {
        if (game.stage == "REGULAR" && (game.status != "FINAL" || game.scheduledGameId <= 0))
            return {{}, "1989 playoff seeding requires a finalized regular season."};
    }
    const auto standings = projectPffStandings(teams, schedule);
    if (!standings.success)
        return {{}, standings.errorMessage};
    if (standings.rows.size() != 28 ||
        std::any_of(standings.rows.begin(), standings.rows.end(), [](const Row& row)
        {
            return row.wins + row.losses + row.ties != 16;
        }))
    {
        return {{}, "1989 playoff seeding requires 28 teams with 16 final games each."};
    }

    ClubGames games;
    for (const auto& game : schedule)
    {
        if (game.stage != "REGULAR")
            continue;
        games[game.awayTeamSeasonId].push_back(
            {teamById.at(game.homeTeamSeasonId), game.awayScore, game.homeScore});
        games[game.homeTeamSeasonId].push_back(
            {teamById.at(game.awayTeamSeasonId), game.homeScore, game.awayScore});
    }

    std::vector<PostseasonSeed> seeds;
    Id divisionId = 0;
    try
    {
        for (const std::string conference : {"AFC", "NFC"})
        {
            std::map<std::string, Group> divisions;
            Group wildCards;
            for (const auto& row : standings.rows)
            {
                if (row.team.conference == conference)
                {
                    divisions[row.team.division].push_back(&row);
                    wildCards.push_back(&row);
                }
            }
            if (divisions.size() != 3 || wildCards.size() < 5)
                return {{}, "Each conference requires three divisions and at least five teams."};

            Group champions;
            std::map<std::string, Id> divisionIds;
            for (const auto& [division, clubs] : divisions)
            {
                divisionIds.emplace(division, ++divisionId);
                const auto best = topRecord(clubs);
                const auto* champion = best.size() == 1 ? best.front() : breakTie(games, best);
                champions.push_back(champion);
                remove(wildCards, champion);
            }
            auto addSeed = [&](const Row& row, int number)
            {
                seeds.push_back({row.team.teamSeasonId, conference, divisionIds.at(row.team.division), number});
            };
            for (int number = 1; number <= 3; ++number)
            {
                const auto best = topRecord(champions);
                const auto* selected = best.size() == 1 ? best.front() : breakTie(games, best);
                addSeed(*selected, number);
                remove(champions, selected);
            }
            for (int number = 4; number <= 5; ++number)
            {
                const auto best = topRecord(wildCards);
                const auto* selected = best.size() == 1 ? best.front() : breakTie(games, best, true);
                addSeed(*selected, number);
                // A restart narrows the current decision only. The next berth
                // starts with every original contender except the club just seeded.
                remove(wildCards, selected);
            }
        }
    }
    catch (const std::domain_error& error)
    {
        return {{}, error.what()};
    }
    return {std::move(seeds), {}};
}
