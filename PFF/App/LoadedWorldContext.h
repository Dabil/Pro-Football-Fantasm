#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

// -----------------------------------------------------------------------------
// LoadedWorldContext
// -----------------------------------------------------------------------------
//
// Small application-level description of the GCE World currently opened by
// Pro Football Fantasm.
//
// This is NOT a database connection, repository, cache of football data, or a
// replacement for WorldDatabase.
//
// WorldDatabase owns persistence.
// PFFApplication owns which World is currently active in the running program.
// Screens/services may use this context to know which World database and active
// CompetitionSeason they should query.
//
// Keep this object intentionally small. Data that naturally changes during play
// (standings, current week, rosters, injuries, statistics, schedule results,
// etc.) should be queried from WorldDB rather than copied into application state.
// -----------------------------------------------------------------------------

struct LoadedWorldContext
{
    // True only after a World has been successfully created/loaded and this
    // structure has been populated from that validated World.
    bool loaded = false;

    // Physical World.db currently active in the application.
    //
    // Example:
    //   Data/World/My1989World/World.db
    std::filesystem::path databasePath;

    // Stable World identity returned by GCE WorldDatabase.
    std::int64_t worldId = 0;
    std::string worldKey;
    std::string worldName;

    // CompetitionSeason currently selected by the application.
    //
    // PFF Classic initially has one active professional season, but keeping this
    // explicit avoids baking a one-season assumption into the application state.
    std::int64_t activeCompetitionSeasonId = 0;

    // Snapshot of the World's canonical clock when the context was last loaded
    // or refreshed. WorldDB remains authoritative; this value must be refreshed
    // after World advancement rather than treated as an independent clock.
    std::string currentTimeUtc;

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return loaded;
    }

    [[nodiscard]] bool hasActiveCompetitionSeason() const noexcept
    {
        return loaded && activeCompetitionSeasonId != 0;
    }

    // Returns this object to the application's "no World loaded" state.
    void clear()
    {
        loaded = false;
        databasePath.clear();

        worldId = 0;
        worldKey.clear();
        worldName.clear();

        activeCompetitionSeasonId = 0;
        currentTimeUtc.clear();
    }
};

