#include "App/PFFApplication.h"

#include "App/ScreenManager.h"

#include "Screens/PffGameScreen.h"
#include "Screens/Developer/RendererDiagnosticsScreen.h"
#include "Screens/Developer/TerminalCapabilitiesScreen.h"

#include <memory>

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

    assetLibrary().registerAlias("pff.field", "XP/Field.xp");
    assetLibrary().registerAlias("pff.scoreboard", "XP/ScoreBoard.xp");
    assetLibrary().registerAlias("pff.scoreboard_font", "XP/ScoreBoardFont.xp");
}

bool PFFApplication::onInitialize()
{
    screenManager().clearScreens();

    if (m_validationScreenStart == StartupValidationScreenPreference::ValidationStartTrue)
    {
        screenManager().pushScreen(
            std::make_unique<TerminalCapabilitiesScreen>(&renderer()));
    }
    else
    {
        screenManager().pushScreen(
            std::make_unique<PffGameScreen>(assetLibrary()));
    }

    requestRender();

    return true;
}

