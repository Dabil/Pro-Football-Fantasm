#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// TEAMDATA-STAGE2-2026-11-03. Generic World editing, no SQL/UI ownership exposed.
enum class WorldEditError
{
    None,
    UnsupportedSchema,
    UnsupportedFeature,
    TeamNotActive,
    NotAuthorized,
    SharedVenueReadOnly,
    StaleRevision,
    StaleRoleHolder,
    PersonUnavailable,
    ControlConflict,
    RoleIncompatible,
    InvalidValue,
    PeriodUnavailable,
    CalendarConflict,
    IoError,
    IntegrityFailure
};
template <class T> struct WorldEditResult
{
    bool success = false;
    WorldEditError error = WorldEditError::None;
    std::string message;
    T value{};
    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};
class WorldActor
{
  public:
    WorldActor() = default;

  private:
    std::filesystem::path database_;
    std::int64_t world_ = 0, user_ = 0;
    std::string worldKey_, userKey_;
    friend struct WorldEditingAccess;
    friend WorldEditResult<WorldActor> openLocalWorldActor(const std::filesystem::path&);
};
// Trusted local application session: binds the persisted LOCAL_PRIMARY identity.
// Drafts never contain a user ID or a God-mode privilege flag.
WorldEditResult<WorldActor> openLocalWorldActor(const std::filesystem::path& database);
// Read-only mode/permission projection for Commissioner and unsaved team drafts.
struct WorldEditState
{
    std::int64_t worldId = 0, revision = 0;
    bool godMode = false, canEnableGodMode = false;
};
WorldEditResult<WorldEditState> loadWorldEditState(const WorldActor&);
struct WorldPersonChoice
{
    std::int64_t personId = 0;
    std::string displayName;
};
struct WorldStaffPosition
{
    std::int64_t positionId = 0, roleId = 0, assignmentId = 0, personId = 0;
    std::string roleKey, displayName, personName;
    bool editable = false, userControlled = false;
};
struct WorldTeamData
{
    std::int64_t worldId = 0, organizationId = 0, teamSeasonId = 0, venueId = 0, revision = 0;
    std::string city, nickname, displayName, venueName, currentTimeUtc;
    std::optional<std::string> surface;
    bool managed = false, godMode = false, canManage = false, canEditIdentity = false, canEditVenue = false,
         calendarReady = false;
    std::int64_t otherVenueUsers = 0;
    std::vector<WorldStaffPosition> staff;
    std::vector<std::string> warnings;
};
struct WorldNewPerson
{
    std::string displayName;
    std::optional<std::string> firstName, lastName;
};
enum class WorldStaffActionKind
{
    Keep,
    Remove,
    Replace
};
struct WorldStaffAction
{
    std::int64_t positionId = 0, expectedAssignmentId = 0;
    WorldStaffActionKind kind = WorldStaffActionKind::Keep;
    std::optional<std::int64_t> personId;
    std::optional<WorldNewPerson> newPerson;
};
struct WorldTeamDataDraft
{
    std::int64_t worldId = 0, organizationId = 0, teamSeasonId = 0, expectedRevision = 0;
    std::optional<std::string> city, nickname, venueName;
    // Outer unset=unchanged; inner unset=clear to unknown.
    // To clear explicitly: draft.surface.emplace(std::nullopt).
    std::optional<std::optional<std::string>> surface;
    std::optional<bool> managed;
    std::vector<WorldStaffAction> staff;
};
struct WorldEditCommit
{
    std::int64_t worldId = 0, revision = 0;
    bool changed = false;
    std::string currentTimeUtc, worldKey;
    std::vector<std::int64_t> organizations, teamSeasons, people, venues;
};
struct WorldVenueHistoryEntry
{
    bool baseline = false, closed = false, hasNetChange = false;
    std::int64_t periodId = 0;
    std::string periodKey, startsUtc, endsUtc, openingName, finalName;
    std::optional<std::string> openingSurface, finalSurface;
};
WorldEditResult<WorldTeamData> loadTeamData(const WorldActor&, std::int64_t teamSeasonId);
WorldEditResult<WorldTeamData> previewTeamManagement(const WorldActor&, std::int64_t teamSeasonId,
                                                     bool enabled);
WorldEditResult<std::vector<WorldPersonChoice>> listAvailablePeople(const WorldActor&,
                                                                    std::int64_t organizationId,
                                                                    std::size_t offset = 0,
                                                                    std::size_t limit = 100);
WorldEditResult<WorldEditCommit> saveTeamData(const WorldActor&, const WorldTeamDataDraft&);
WorldEditResult<WorldEditCommit> setTeamManagement(const WorldActor&, std::int64_t organizationId,
                                                   bool enabled, std::int64_t expectedRevision);
WorldEditResult<WorldEditCommit> setWorldEditMode(const WorldActor&, bool enabled,
                                                  std::int64_t expectedRevision);
WorldEditResult<std::vector<WorldVenueHistoryEntry>> listVenueHistory(const WorldActor&, std::int64_t venueId,
                                                                      std::size_t offset = 0,
                                                                      std::size_t limit = 100);
// Domain hooks: forward-only clock closure and immutable venue evidence. They do
// not simulate or finalize a game; the future resolver must call capture before play.
WorldEditResult<WorldEditCommit> advanceWorldClock(const WorldActor&, const std::string& utc,
                                                   std::int64_t expectedRevision);
WorldEditResult<WorldEditCommit> captureGameVenueEvidence(const WorldActor&, std::int64_t gameId,
                                                          std::int64_t expectedRevision);
