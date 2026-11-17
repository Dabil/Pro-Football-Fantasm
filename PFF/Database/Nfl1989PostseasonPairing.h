#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class PostseasonRound
{
    WildCard,
    Divisional,
    ConferenceChampionship,
    SuperBowl
};

struct PostseasonSeed
{
    std::int64_t teamSeasonId;
    std::string conference; // AFC or NFC
    std::int64_t divisionId;
    int seed;
};

struct PostseasonPairing
{
    PostseasonRound round;
    std::int64_t awayTeamSeasonId;
    std::int64_t homeTeamSeasonId;
    bool neutralSite;
    std::optional<std::int64_t> winnerTeamSeasonId;
};

class Nfl1989PostseasonPairing
{
public:
    // Accepts both conferences' original seeds and completed games in any order.
    // Returns outstanding games in the earliest unfinished round, or an empty
    // list after the Super Bowl. Inconsistent input throws std::invalid_argument.
    [[nodiscard]] static std::vector<PostseasonPairing> nextRound(
        const std::vector<PostseasonSeed>& seeds,
        const std::vector<PostseasonPairing>& completed = {});
};
