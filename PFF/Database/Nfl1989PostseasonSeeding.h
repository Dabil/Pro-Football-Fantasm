#pragma once

#include "Nfl1989PostseasonPairing.h"
#include "GCE/Database/WorldDatabase.h"

struct Nfl1989SeedingResult
{
    std::vector<PostseasonSeed> seeds;
    std::string errorMessage;

    [[nodiscard]] bool succeeded() const noexcept { return errorMessage.empty(); }
};

class Nfl1989PostseasonSeeding
{
public:
    // Supply one season's complete team list and schedule. Uses final regular-
    // season results. An unresolved rule or invalid input returns no seeds.
    [[nodiscard]] static Nfl1989SeedingResult seed(
        const std::vector<WorldTeamRecord>& teams,
        const std::vector<WorldScheduleGameRecord>& schedule);
};
