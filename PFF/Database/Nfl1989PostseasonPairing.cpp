#include "Nfl1989PostseasonPairing.h"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

namespace
{
using Conference = std::array<const PostseasonSeed*, 5>;
using Field = std::array<Conference, 2>;
using RoundWinners = std::array<std::vector<const PostseasonSeed*>, 2>;

Field validateSeeds(const std::vector<PostseasonSeed>& seeds)
{
    Field field{};
    std::set<std::int64_t> teamIds;
    for (const auto& seed : seeds)
    {
        if ((seed.conference != "AFC" && seed.conference != "NFC") ||
            seed.seed < 1 || seed.seed > 5 || seed.divisionId <= 0 ||
            seed.teamSeasonId <= 0 || !teamIds.insert(seed.teamSeasonId).second)
        {
            throw std::invalid_argument("Invalid 1989 postseason seed.");
        }
        auto& slot = field[seed.conference == "AFC" ? 0 : 1][seed.seed - 1];
        if (slot)
            throw std::invalid_argument("Duplicate 1989 conference seed.");
        slot = &seed;
    }
    for (const auto& conference : field)
    {
        if (std::find(conference.begin(), conference.end(), nullptr) != conference.end())
            throw std::invalid_argument("Each conference requires five postseason seeds.");

        const std::set<std::int64_t> divisions = {
            conference[0]->divisionId, conference[1]->divisionId, conference[2]->divisionId
        };
        if (divisions.size() != 3 || !divisions.count(conference[3]->divisionId) ||
            !divisions.count(conference[4]->divisionId))
        {
            throw std::invalid_argument("Each conference requires three distinct division winners.");
        }
    }
    return field;
}
}

std::vector<PostseasonPairing> Nfl1989PostseasonPairing::nextRound(
    const std::vector<PostseasonSeed>& seeds,
    const std::vector<PostseasonPairing>& completed)
{
    const auto field = validateSeeds(seeds);
    std::size_t acceptedResults = 0;
    RoundWinners previous;
    for (const auto round : { PostseasonRound::WildCard, PostseasonRound::Divisional,
                              PostseasonRound::ConferenceChampionship, PostseasonRound::SuperBowl })
    {
        std::vector<PostseasonPairing> pending;
        RoundWinners winners;
        auto addGame = [&](std::size_t conference, const PostseasonSeed* away,
                            const PostseasonSeed* home, bool neutral = false)
        {
            const PostseasonPairing game{ round, away->teamSeasonId, home->teamSeasonId,
                                          neutral, std::nullopt };
            const auto result = std::find_if(completed.begin(), completed.end(),
                [&](const PostseasonPairing& candidate)
                {
                    return candidate.round == game.round &&
                           candidate.awayTeamSeasonId == game.awayTeamSeasonId &&
                           candidate.homeTeamSeasonId == game.homeTeamSeasonId &&
                           candidate.neutralSite == game.neutralSite;
                });
            if (result == completed.end())
            {
                pending.push_back(game);
                return;
            }
            if (result->winnerTeamSeasonId == away->teamSeasonId)
                winners[conference].push_back(away);
            else if (result->winnerTeamSeasonId == home->teamSeasonId)
                winners[conference].push_back(home);
            else
                throw std::invalid_argument("Postseason winner must be a participating team.");
            ++acceptedResults;
        };

        if (round == PostseasonRound::SuperBowl)
        {
            // The 1989 fixture designates the AFC champion as home at a neutral site.
            addGame(0, previous[1][0], previous[0][0], true);
        }
        else
        {
            for (std::size_t i = 0; i < field.size(); ++i)
            {
                const auto [first, second, third, fourth, fifth] = field[i];
                if (round == PostseasonRound::WildCard)
                    addGame(i, fifth, fourth);
                else if (round == PostseasonRound::Divisional)
                {
                    const auto wildCard = previous[i][0];
                    const bool sameDivision = wildCard->divisionId == first->divisionId;
                    addGame(i, sameDivision ? third : wildCard, first);
                    addGame(i, sameDivision ? wildCard : third, second);
                }
                else
                {
                    const auto a = previous[i][0];
                    const auto b = previous[i][1];
                    const bool aHosts = a->seed < b->seed;
                    addGame(i, aHosts ? b : a, aHosts ? a : b);
                }
            }
        }

        if (!pending.empty() || round == PostseasonRound::SuperBowl)
        {
            if (acceptedResults != completed.size())
                throw std::invalid_argument("Duplicate, unmatched, or premature postseason result.");
            return pending;
        }
        previous = winners;
    }
    return {};
}
