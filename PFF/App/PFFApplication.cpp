#include "App/PFFApplication.h"

#include "App/ScreenManager.h"

#include "GCE/Database/WorldDatabase.h"

#include "Screens/MainScreen.h"
#include "Screens/Developer/RendererDiagnosticsScreen.h"
#include "Screens/Developer/TerminalCapabilitiesScreen.h"

// PFF Screen Includes
#include "Screens/IntroScreen.h"
#include "Screens/GameScreen.h"
#include "Screens/CommissionersOfficeScreen.h"
#include "Screens/LeaguePlayScreen.h"
#include "Screens/TeamsAndStandingsScreen.h"
#include "Screens/LeagueScheduleScreen.h"
#include "Screens/LeagueLeadersScreen.h"
#include "Screens/TradeDeskScreen.h"

// C++ Library includes
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view NewLeagueCommand = "action:pff:new_league";
    constexpr std::int32_t PffClassicInitialSeason = 1989;

    const std::filesystem::path PffClassicCoreDatabase =
        std::filesystem::path("Data") / "1989_PFF_Core.db";

    const std::filesystem::path WorldRoot =
        std::filesystem::path("Data") / "World";

    struct NewWorldDestination
    {
        std::string worldName;
        std::filesystem::path databasePath;
    };

    // The Commissioner UI does not have a league-name entry form yet, so this
    // first vertical slice chooses a safe automatic name. Existing World
    // folders are never overwritten:
    //
    //   Data/World/PFF_Classic_1989/World.db
    //   Data/World/PFF_Classic_1989_2/World.db
    //   Data/World/PFF_Classic_1989_3/World.db
    //
    // A later New League form can replace this helper without changing GCE.
    bool chooseNewWorldDestination(NewWorldDestination& destination)
    {
        constexpr int MaxAutomaticWorldNumber = 9999;

        for (int worldNumber = 1;
            worldNumber <= MaxAutomaticWorldNumber;
            ++worldNumber)
        {
            std::string folderName = "PFF_Classic_1989";
            std::string worldName = "PFF Classic 1989";

            if (worldNumber > 1)
            {
                folderName += "_" + std::to_string(worldNumber);
                worldName += " #" + std::to_string(worldNumber);
            }

            const std::filesystem::path worldFolder =
                WorldRoot / folderName;

            std::error_code error;
            const bool alreadyExists =
                std::filesystem::exists(worldFolder, error);

            if (error)
            {
                return false;
            }

            if (alreadyExists)
            {
                continue;
            }

            destination.worldName = std::move(worldName);
            destination.databasePath = worldFolder / "World.db";
            return true;
        }

        return false;
    }
}

PFFApplication::PFFApplication(
    StartupRendererSelection rendererSelection,
    StartupValidationScreenPreference validationScreenSelection,
    const StartupDiagnosticsContext& startupDiagnostics)
    : ApplicationHost(rendererSelection, validationScreenSelection, startupDiagnostics)
    , m_validationScreenStart(validationScreenSelection)
{
}

bool PFFApplication::hasLoadedWorld() const noexcept
{
    return m_loadedWorldContext.isLoaded();
}

LoadedWorldContext& PFFApplication::loadedWorldContext() noexcept
{
    return m_loadedWorldContext;
}

const LoadedWorldContext& PFFApplication::loadedWorldContext() const noexcept
{
    return m_loadedWorldContext;
}

void PFFApplication::setLoadedWorldContext(LoadedWorldContext context)
{
    m_loadedWorldContext = std::move(context);
    requestRender();
}

void PFFApplication::clearLoadedWorldContext()
{
    m_loadedWorldContext.clear();
    requestRender();
}

bool PFFApplication::handlePffCommandId(std::string_view commandId)
{
    if (commandId == NewLeagueCommand)
    {
        const bool created = createNewLeague();

        // New League is an application action rather than a route transition.
        // Request a redraw whether creation succeeded or failed so the current
        // screen can reflect any local feedback/state changes.
        requestRender();
        return created;
    }

    // Route/navigation commands remain owned by ApplicationHost.
    return handleCommandId(commandId);
}

bool PFFApplication::createNewLeague()
{
    NewWorldDestination destination;

    if (!chooseNewWorldDestination(destination))
    {
        return false;
    }

    WorldDatabaseCreateRequest request;
    request.coreDatabase = PffClassicCoreDatabase;
    request.outputDatabase = destination.databasePath;
    request.worldName = destination.worldName;

    // Let generic GCE generate the stable WorldKey. The PFF UI should not
    // invent database identity values.
    request.worldKey.clear();

    request.season = PffClassicInitialSeason;

    // New League must never destroy an existing World. The automatic
    // destination helper already selects a fresh folder, and this keeps the
    // persistence API's safety invariant intact as well.
    request.overwriteExisting = false;

    const WorldDatabaseCreateResult createResult =
        createWorldDatabase(request);

    if (!createResult.succeeded())
    {
        return false;
    }

    // Creation succeeded. Now prove the save through the same load/validation
    // path used by the Commissioner's Office Load League workflow.
    return loadWorldIntoContext(createResult.databasePath);
}

bool PFFApplication::loadWorldIntoContext(
    const std::filesystem::path& databasePath)
{
    const WorldDatabaseLoadResult loadResult =
        loadWorldDatabase(databasePath);

    if (!loadResult.succeeded())
    {
        return false;
    }

    if (loadResult.competitionSeasons.empty())
    {
        return false;
    }

    // Prefer an ACTIVE competition season. If a future World has no ACTIVE
    // marker, fall back to the first valid season so the application still has
    // a concrete navigation context.
    auto activeSeason = std::find_if(
        loadResult.competitionSeasons.begin(),
        loadResult.competitionSeasons.end(),
        [](const WorldCompetitionSeasonRecord& season)
        {
            return season.status == "ACTIVE";
        });

    if (activeSeason == loadResult.competitionSeasons.end())
    {
        activeSeason = loadResult.competitionSeasons.begin();
    }

    LoadedWorldContext context;
    context.loaded = true;
    context.databasePath = databasePath;

    context.worldId = loadResult.world.worldId;
    context.worldKey = loadResult.world.worldKey;
    context.worldName = loadResult.world.worldName;

    context.activeCompetitionSeasonId =
        activeSeason->competitionSeasonId;

    context.currentTimeUtc =
        loadResult.world.currentTimeUtc;

    // Publish only after the complete World has passed normal load validation.
    setLoadedWorldContext(std::move(context));
    return true;
}

void PFFApplication::configureAssetLibrary()
{
    assetLibrary().setAssetsRoot("Assets");

    // GameScreen Assets
    assetLibrary().registerAlias("pff.field", "XP/Field.xp");
    assetLibrary().registerAlias("pff.scoreboard", "XP/ScoreBoard.xp");
    assetLibrary().registerAlias("pff.scoreboard_font", "XP/ScoreBoardFont.xp");

    // MainScreen Assets
    assetLibrary().registerAsset("banner.ansi_shadow", "Fonts/FIGlet/ANSI Shadow.flf");
}

bool PFFApplication::onInitialize()
{
    screenManager().clearScreens();

    registerScreenRoute(
        "pff:main",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<MainScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                },
                [this]()
                {
                    return hasLoadedWorld();
                });
        });

    registerScreenRoute(
        "pff:league_play",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<LeaguePlayScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                });
        });

    registerScreenRoute(
        "pff:commissioners_office",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<CommissionersOfficeScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                },
                [this]()
                {
                    return loadedWorldContext();
                },
                [this](const std::filesystem::path& databasePath)
                {
                    return loadWorldIntoContext(databasePath);
                });
        });

    registerScreenRoute(
        "pff:teams_and_standings",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<TeamsAndStandingsScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                },
                [this]()
                {
                    return loadedWorldContext();
                });
        });

    registerScreenRoute(
        "pff:league_schedule",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<LeagueScheduleScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                },
                [this]()
                {
                    return loadedWorldContext();
                });
        });

    registerScreenRoute(
        "pff:league_leaders",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<LeagueLeadersScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                });
        });

    registerScreenRoute(
        "pff:trade_desk",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<TradeDeskScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                });
        });

    registerScreenRoute(
        "pff:game",
        [this]() -> std::unique_ptr<Screen>
        {
            return std::make_unique<GameScreen>(
                assetLibrary());
        });

    if (m_validationScreenStart ==
        StartupValidationScreenPreference::ValidationStartTrue)
    {
        screenManager().pushScreen(
            std::make_unique<TerminalCapabilitiesScreen>(
                &renderer()));
    }
    else
    {
        screenManager().pushScreen(
            std::make_unique<MainScreen>(
                assetLibrary(),
                [this](std::string_view commandId)
                {
                    return handlePffCommandId(commandId);
                },
                [this]()
                {
                    return hasLoadedWorld();
                }));
    }

    requestRender();

    return true;
}



