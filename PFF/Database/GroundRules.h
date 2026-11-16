#pragma once

#include "GCE/Database/WorldTeamData.h"
#include <array>
#include <limits>

namespace GroundRules
{
enum Feature { PlayClock, Endurance, Fumbles, Injuries, Interceptions, Penalties, WeatherEffects, WildPlays };
inline constexpr std::array<const char*, 8> labels{
    "Play Clock", "Endurance", "Fumbles", "Injuries", "Interceptions", "Penalties", "Weather Effects", "Wild Plays"};
inline constexpr std::array<const char*, 8> keys{
    "PLAY_CLOCK", "ENDURANCE", "FUMBLES", "INJURIES", "INTERCEPTIONS", "PENALTIES", "WEATHER_EFFECTS", "WILD_PLAYS"};
inline constexpr auto clockKey = "PLAY_CLOCK_SECONDS";

struct Settings
{
    std::array<bool, keys.size()> supported{};
    std::array<bool, keys.size()> enabled{};
    int clockSeconds = 30;
};

inline std::map<std::string, std::int64_t> values(const Settings& settings)
{
    std::map<std::string, std::int64_t> result;
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (settings.supported[i]) result.emplace(keys[i], settings.enabled[i]);
    if (settings.supported[PlayClock]) result.emplace(clockKey, settings.clockSeconds);
    return result;
}

inline Settings resolve(const WorldLeagueOptions& options)
{
    Settings settings;
    const auto& profile = options.rulesProfile;
    if (profile == "FirstEdition" || profile == "SecondEdition" || profile == "ThirdEdition")
        settings.supported.fill(true);
    settings.clockSeconds = profile == "SecondEdition" ? 40 : 30;
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        const auto saved = options.values.find(keys[i]);
        settings.enabled[i] = settings.supported[i] && (saved == options.values.end() || saved->second == 1);
    }
    const auto seconds = options.values.find(clockKey);
    if (seconds != options.values.end() && seconds->second > 0 && seconds->second <= (std::numeric_limits<int>::max)())
        settings.clockSeconds = static_cast<int>(seconds->second);
    return settings;
}
}
