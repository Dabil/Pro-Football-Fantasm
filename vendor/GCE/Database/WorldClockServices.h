#pragma once
#include "GCE/Database/WorldTeamData.h"

// CLOCK-SCHEDULER-STAGE1-2026-11-04. Trusted ordinary-World read services.
struct WorldClockReadiness
{
    std::int64_t worldId=0,competitionSeasonId=0,revision=0;
    bool ready=false;
    std::vector<std::string> blockers;
};
struct WorldTradingState
{
    std::int64_t competitionSeasonId=0,revision=0;
    bool known=false,open=false;
    std::string asOfUtc,deadlineUtc;
};
WorldEditResult<WorldClockReadiness> loadClockSchedulerReadiness(const WorldActor&,std::int64_t competitionSeasonId);
WorldEditResult<WorldTradingState> loadWorldTradingState(const WorldActor&,std::int64_t competitionSeasonId);
// A writer must recheck this inside its own future trade transaction. No trade
// completion API is supplied until roster/contract temporal ownership is designed.
