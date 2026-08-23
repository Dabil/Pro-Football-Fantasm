#include "App/PFFApplication.h"

#include "App/ScreenManager.h"

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
#include <memory>
#include <string_view>

PFFApplication::PFFApplication(
    StartupRendererSelection rendererSelection,
    StartupValidationScreenPreference validationScreenSelection,
    const StartupDiagnosticsContext& startupDiagnostics)
    : ApplicationHost(rendererSelection, validationScreenSelection, startupDiagnostics)
    , m_validationScreenStart(validationScreenSelection)
{
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
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
                    return handleCommandId(commandId);
                }));
    }

    requestRender();

    return true;
}

