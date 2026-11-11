#pragma once
#include "GCE/Database/WorldTestGame.h"
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace PffTest
{
using TeamStats=WorldTestTeamStats;
using State=WorldTestRuntimeState;
// This explicitly reduced provider is independent of TUI and can be run headlessly.
class PffGameSession
{
  public:
    PffGameSession(const std::filesystem::path &world, std::int64_t season, std::uint32_t seed = 0);
    ~PffGameSession();
    WorldEditResult<WorldTestMatchup> start(const WorldActor &, std::int64_t revision);
    WorldEditResult<WorldTestMatchup> viewFinal(const WorldActor &, std::int64_t game);
    WorldEditResult<WorldEditCommit> next(const WorldActor &);
    WorldEditResult<WorldEditCommit> save(const WorldActor &);
    WorldEditResult<WorldTestMatchup> resume(const WorldActor &);
    const std::optional<WorldTestQueuedEvent> &interruption() const;
    const State &state() const;
    const WorldTestMatchup &matchup() const;
    const std::vector<std::string> &log() const;
    std::string statistics() const;
    std::string_view taggedHistory() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
} // namespace PffTest
