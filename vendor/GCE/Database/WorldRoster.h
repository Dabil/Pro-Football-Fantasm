#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// ROSTER-2026-11-21: read projection; no time advancement or save mutation.
struct WorldRosterModifier
{
    std::string ratingKey, reason;
    double adjustment = 0;
};
struct WorldRosterRating
{
    std::string key;
    int baseline = 0;
};
struct WorldRosterPlayer
{
    std::int64_t playerSeasonId = 0;
    std::string firstName, lastName, position, cardType, rosterStatus, healthStatus, experienceOrigin;
    int jersey = 0, depth = 0, yearsInLeague = 0;
    std::optional<int> age, heightInches, weightPounds, injuryRating, gamesRemaining;
    std::vector<WorldRosterRating> ratings;
    std::vector<WorldRosterModifier> modifiers;
};
struct WorldRosterLoadResult
{
    bool success = false;
    std::string teamName, errorMessage;
    std::vector<WorldRosterPlayer> players;
};
WorldRosterLoadResult loadWorldRoster(const std::filesystem::path& worldDatabase,
    std::int64_t competitionSeasonId, std::int64_t teamSeasonId);
