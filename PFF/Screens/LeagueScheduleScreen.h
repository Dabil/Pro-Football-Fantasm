#pragma once

#include "Screens/Screen.h"
#include "App/LoadedWorldContext.h"
#include "GCE/Database/WorldDatabase.h"
#include "GCE/Database/WorldScheduler.h"
#include "Database/GenericTestGame.h"
#include "Assets/AssetLibrary.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Surface;

namespace Composition
{
    class PageComposer;
}

class LeagueScheduleScreen : public Screen
{
public:
    explicit LeagueScheduleScreen(
        Assets::AssetLibrary& assetLibrary,
        std::function<bool(std::string_view)> commandDispatcher,
        std::function<LoadedWorldContext()> loadedWorldContextProvider,
        std::function<void(const LoadedWorldContext&,std::int64_t)> openTeam = {},
        std::function<void(const WorldEditCommit&)> committed = {},
        std::function<void(std::unique_ptr<PffTest::GenericTestGame>,WorldActor)> openGame = {}, std::int64_t teamSeasonId = 0);

    ~LeagueScheduleScreen();

    void onEnter() override;
    void onExit() override;
    bool handleEvent(const Input::Event& event) override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    bool controlsTeam(std::int64_t teamId) const;
    void openResult(std::int64_t gameId);
    void buildButtons();
    void positionButtons(const Rect& navigationPane, const Rect& footer);
    void handleButtonActivation(const ButtonActivationResult& result);

    void loadSchedule();
    void loadGameWindow();
    void beginGameWindow();
    void runBatch();
    std::string nextTeamKickoff() const;
    void continueGameWindow();
    void publishAdvance(const WorldAdvanceOutcome&);
    void drawGameWindowStatus(Composition::PageComposer&, const Rect&);
    void refreshLeagueContext();
    void updateNavigationButtons();
    void drawCurrentWeek(
        Composition::PageComposer& page,
        std::string_view panelRegionName,
        std::string_view contentRegionName);
    void drawFooter(Composition::PageComposer& page, const Rect& footer) const;

    [[nodiscard]] std::int32_t currentWeek() const noexcept;

private:
    Assets::AssetLibrary& m_assetLibrary;

    // Title Text
    std::string m_TitleBannerFontKey = "banner.ansi_shadow";
    TextObject m_LeagueScheduleTitleObject;
    Assets::LoadBannerFontResult m_fontResult;

    // Buttons
    std::function<bool(std::string_view)> m_commandDispatcher;
    ContainerWidget m_root;
    Button* m_leaguePlayButton = nullptr;
    Button* m_lookAheadButton = nullptr;
    Button* m_lookBackButton = nullptr;
    Button* m_nextGameButton = nullptr;
    std::function<void(const WorldEditCommit&)> m_committed;
    std::function<void(std::unique_ptr<PffTest::GenericTestGame>,WorldActor)> m_openGame;
    WorldActor m_actor;
    std::int64_t m_teamSeasonId = 0;
    bool m_controlsSelectedTeam = false;
    WorldGameWindow m_gameWindow;
    bool m_gameWindowReady = false, m_advancing = false;
    std::string m_windowMessage;
    std::string m_batchKickoff;
    std::unique_ptr<PffTest::GenericTestGame> m_backgroundGame;
    bool m_returningFromGame = false;
    bool m_batchStopped = false;

    // Data belongs to the selected saved league and competition season.
    std::function<LoadedWorldContext()> m_loadedWorldContextProvider;
    LoadedWorldContext m_dataContext;

    std::vector<std::int32_t> m_availableWeeks;
    std::vector<WorldScheduleGameRecord> m_scheduleGames;
    std::size_t m_currentWeekIndex = 0;
    std::string m_scheduleLoadMessage;
    bool m_scheduleLoaded = false;
    std::string m_gameTimeLabel;
    int m_scrollOffset = 0;
    int m_maxScrollOffset = 0;
    int m_visibleRows = 1;
    Rect m_scheduleBounds;
    Rect m_windowStatusBounds;
    // Static schedule/header/footer drawing; buttons remain live.
    std::function<void(const LoadedWorldContext&,std::int64_t)> m_openTeam;
    std::vector<Widget*> m_teamLinks;
    std::unique_ptr<Surface> m_scheduleCache;
};

class TeamScheduleScreen final : public LeagueScheduleScreen
{
public:
    using LeagueScheduleScreen::LeagueScheduleScreen;
};

