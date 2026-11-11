#pragma once
#include <array>
#include <cstdint>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// GAME-EXECUTION-TASK3-2026-11-04. Versioned state for the explicitly reduced
// GameTime provider. No C++ object memory/pointers are persisted.
struct WorldTestTeamStats
{
    int firstDowns = 0, rushes = 0, rushYards = 0, attempts = 0, completions = 0, passYards = 0;
    int interceptions = 0, fumblesLost = 0, punts = 0, puntYards = 0, sacks = 0, sackYards = 0;
    std::array<int, 5> quarters{};
    int score() const
    {
        int n = 0;
        for (auto q : quarters)
            n += q;
        return n;
    }
};
struct WorldTestRuntimeState
{
    int quarter = 1, remaining = 900, elapsed = 0, offense = 1, spot = 20, down = 1, lineToGain = 30;
    std::int64_t generation = 0, playOrdinal = 0;
    bool final = false;
    int pendingKick = -1;
    std::array<WorldTestTeamStats, 2> teams;
};
struct WorldTestSavedState
{
    std::int64_t gameId = 0;
    std::string inputHash, randomState;
    WorldTestRuntimeState game;
    std::array<int, 2> planTier{};
    std::vector<std::string> log;
    std::string replayHistory;
};
namespace WorldTestStateCodec
{
inline constexpr const char *Format = "GENERIC_GAME_STATE_V3";
inline constexpr const char *InitialFormat = "GAME_TIME_INITIAL_V1";
inline constexpr const char *StepFormat = "GAME_TIME_STEP_V1";
inline constexpr std::size_t MaxBytes = 16u * 1024u * 1024u;
inline constexpr std::array<int WorldTestTeamStats::*, 12> Stats = {
    &WorldTestTeamStats::firstDowns,    &WorldTestTeamStats::rushes,      &WorldTestTeamStats::rushYards,
    &WorldTestTeamStats::attempts,      &WorldTestTeamStats::completions, &WorldTestTeamStats::passYards,
    &WorldTestTeamStats::interceptions, &WorldTestTeamStats::fumblesLost, &WorldTestTeamStats::punts,
    &WorldTestTeamStats::puntYards,     &WorldTestTeamStats::sacks,       &WorldTestTeamStats::sackYards};
inline void insist(bool good)
{
    if (!good)
        throw std::runtime_error("INCOMPATIBLE_CHECKPOINT: invalid GameTime state/codec");
}
inline void validate(const WorldTestSavedState &s)
{
    const auto &g = s.game;
    insist(s.gameId > 0 && s.inputHash.size() == 64 &&
           s.inputHash.find_first_not_of("0123456789abcdef") == std::string::npos);
    insist(g.generation >= 0 && g.playOrdinal >= 0 && g.playOrdinal <= g.generation && g.quarter >= 1 &&
           g.quarter <= 5 && g.remaining >= 0 && g.remaining <= 900 &&
           g.elapsed == (g.quarter - 1) * 900 + 900 - g.remaining && g.offense >= 0 && g.offense <= 1 &&
           g.pendingKick >= -1 && g.pendingKick <= 1 && g.spot >= 1 && g.spot <= 99 && g.down >= 1 && g.down <= 4 && g.lineToGain >= g.spot &&
           g.lineToGain <= 100 && g.final == ((g.quarter == 4 && g.remaining == 0 && g.teams[0].score() != g.teams[1].score()) || (g.quarter == 5 && (g.remaining == 0 || g.teams[0].score() != g.teams[1].score()))));
    for (const auto &team : g.teams)
    {
        for (std::size_t n = 0; n < Stats.size(); ++n)
            insist(team.*Stats[n] >= ((n == 2 || n == 5) ? -100000 : 0) && team.*Stats[n] <= 100000);
        for (std::size_t q = 0; q < team.quarters.size(); ++q)
            insist(team.quarters[q] >= 0 && team.quarters[q] <= 100000 &&
                   (q < static_cast<std::size_t>(g.quarter) || team.quarters[q] == 0));
    }
    insist(s.planTier[0] >= -2 && s.planTier[0] <= 2 && s.planTier[1] >= -2 && s.planTier[1] <= 2);
    insist(!s.randomState.empty() && s.randomState.size() < 20000 && s.log.size() <= 10000);
    std::istringstream in(s.randomState);
    in.imbue(std::locale::classic());
    std::mt19937 rng;
    in >> rng;
    insist(!in.fail());
    in >> std::ws;
    insist(in.eof());
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << rng;
    insist(out.str() == s.randomState);
    std::size_t total = s.randomState.size();
    for (const auto &line : s.log)
    {
        insist(line.size() <= 1024u * 1024u);
        total += line.size();
    }
    insist(total < MaxBytes);
}
inline void part(std::string &out, const std::string &value)
{
    out += std::to_string(value.size()) + ":" + value;
}
inline std::string encode(const WorldTestSavedState &s)
{
    validate(s);
    std::string out;
    auto p = [&](const std::string &v) { part(out, v); };
    auto n = [&](std::int64_t v) { p(std::to_string(v)); };
    p(Format);
    n(s.gameId);
    p(s.inputHash);
    const auto &g = s.game;
    n(g.generation);
    n(g.quarter);
    n(g.remaining);
    n(g.elapsed);
    n(g.offense);
    n(g.spot);
    n(g.down);
    n(g.lineToGain);
    n(g.final);
    n(g.playOrdinal);
    n(g.pendingKick);
    for (const auto &team : g.teams)
    {
        for (auto member : Stats)
            n(team.*member);
        for (auto q : team.quarters)
            n(q);
    }
    n(s.planTier[0]);
    n(s.planTier[1]);
    p(s.randomState);
    n(s.log.size());
    for (const auto &line : s.log)
        p(line);
    if (!s.replayHistory.empty())
        p(s.replayHistory);
    insist(out.size() <= MaxBytes);
    return out;
}
inline WorldTestSavedState decode(const std::string &bytes)
{
    insist(bytes.size() <= MaxBytes);
    std::size_t at = 0;
    auto p = [&]() {
        const auto colon = bytes.find(':', at);
        insist(colon != std::string::npos && colon - at < 10);
        const auto token = bytes.substr(at, colon - at);
        insist(!token.empty() && token.find_first_not_of("0123456789") == std::string::npos);
        const auto length = std::stoull(token);
        at = colon + 1;
        insist(length <= bytes.size() - at);
        auto value = bytes.substr(at, length);
        at += length;
        return value;
    };
    auto number = [&]() {
        const auto v = p();
        std::size_t used = 0;
        const auto n = std::stoll(v, &used);
        insist(used == v.size());
        return n;
    };
    auto integer = [&]() {
        const auto n = number();
        insist(n >= -100000 && n <= 100000);
        return static_cast<int>(n);
    };
    WorldTestSavedState s;
    insist(p() == Format);
    s.gameId = number();
    s.inputHash = p();
    auto &g = s.game;
    g.generation = number();
    g.quarter = integer();
    g.remaining = integer();
    g.elapsed = integer();
    g.offense = integer();
    g.spot = integer();
    g.down = integer();
    g.lineToGain = integer();
    auto boolean = [&]() {
        const auto v = integer();
        insist(v == 0 || v == 1);
        return v != 0;
    };
    g.final = boolean();
    g.playOrdinal = number();
    g.pendingKick = integer();
    for (auto &team : g.teams)
    {
        for (auto member : Stats)
            team.*member = integer();
        for (auto &q : team.quarters)
            q = integer();
    }
    s.planTier[0] = integer();
    s.planTier[1] = integer();
    s.randomState = p();
    const auto count = number();
    insist(count >= 0 && count <= 10000);
    for (std::int64_t i = 0; i < count; ++i)
        s.log.push_back(p());
    if (at < bytes.size())
        s.replayHistory = p();
    insist(at == bytes.size());
    validate(s);
    insist(encode(s) == bytes);
    return s;
}
} // namespace WorldTestStateCodec
