#pragma once
#include "GCE/Database/WorldTeamData.h"
#include <stdexcept>
struct sqlite3;
// Internal transaction participant for the trusted future game finalizer, not a UI command.
// Caller owns BEGIN IMMEDIATE, canonical game/history/statistics publication and revision CAS.
// On any exception caller MUST roll back. No connection, commit or clock jump is performed here.
namespace WorldWeekInternal
{
    struct TransactionError : std::runtime_error
    {
        WorldEditError error;
        TransactionError(WorldEditError code, const std::string& message)
            : std::runtime_error(message), error(code)
        {
        }
    };
    void recordFinalizedResult(sqlite3* db, std::int64_t worldId, std::int64_t gameId,
                               std::int64_t newRevision, const std::string& simulationUtc,
                               const std::string& finalizationKey, const std::string& resultFingerprint,
                               WorldEditCommit& result);
} // namespace WorldWeekInternal
